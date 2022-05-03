#include <iostream>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Log.h"
#include "http/Client.h"
#include "net/NetError.h"
#include "net/Server.h"

namespace Algiz {
	Server::Server(int af_, const std::string &ip_, uint16_t port_, size_t thread_count, size_t chunk_size):
	af(af_), ip(ip_), port(port_), chunkSize(chunk_size), threadCount(thread_count) {
		if (thread_count < 1)
			throw std::invalid_argument("Cannot instantiate a Server with a thread count of zero");
	}

	Server::~Server() {
		stop();
		for (auto &[descriptor, buffer]: bufferEvents)
			bufferevent_free(buffer);
	}

	Server::Worker::Worker(Server &server_, size_t buffer_size):
	server(server_), bufferSize(buffer_size), buffer(std::make_unique<char[]>(buffer_size)),
	base(event_base_new()) {
		if (base == nullptr)
			throw std::runtime_error("Couldn't allocate a new event_base");	

		acceptEvent = event_new(base, -1, EV_PERSIST, &worker_acceptcb, this);
		if (acceptEvent == nullptr)
			throw std::runtime_error("Couldn't allocate acceptEvent");
		if (event_add(acceptEvent, nullptr) < 0)
			throw std::runtime_error("Couldn't add acceptEvent: " + std::string(strerror(errno)));
	}

	Server::Worker::~Worker() {
		event_free(acceptEvent);
		event_base_free(base);
	}

	void Server::Worker::work(size_t id) {
		event_base_loop(base, EVLOOP_NO_EXIT_ON_EMPTY);
	}

	void Server::Worker::stop() {
		event_base_loopexit(base, nullptr);
	}

	void Server::makeName() {
		name4 = {.sin_family  = AF_INET,  .sin_port  = htons(port)};
		name6 = {.sin6_family = AF_INET6, .sin6_port = htons(port)};

		int status;

		if (af == AF_INET) {
			name = reinterpret_cast<sockaddr *>(&name4);
			nameSize = sizeof(name4);
			status = inet_pton(AF_INET, ip.c_str(), &name4.sin_addr.s_addr);
		} else if (af == AF_INET6) {
			name = reinterpret_cast<sockaddr *>(&name6);
			nameSize = sizeof(name6);
			status = inet_pton(AF_INET6, ip.c_str(), &name6.sin6_addr);
		} else
			throw std::invalid_argument("Unsupported or invalid address family: " + std::to_string(af));

		if (status != 1)
			throw std::invalid_argument("Couldn't parse IP address \"" + ip + "\"");
	}

	int Server::getDescriptor(int client) {
		int descriptor;
		{
			auto lock = lockClients();
			descriptor = descriptors.at(client);
		}
		return descriptor;
	}

	bufferevent * Server::getBufferEvent(int descriptor) {
		bufferevent *buffer_event;
		{
			auto lock = lockDescriptors();
			buffer_event = bufferEvents.at(descriptor);
		}
		return buffer_event;
	}

	void Server::handleMessage(int client, const std::string_view &message) {
		if (messageHandler)
			messageHandler(client, message);
		else
			std::cerr << "Server: got message from client " << client << ": \"" << message << "\"\n";
	}

	ssize_t Server::send(int client, std::string_view message) {
		return bufferevent_write(getBufferEvent(getDescriptor(client)), message.begin(), message.size());
	}

	ssize_t Server::send(int descriptor, const std::string &message) {
		return send(descriptor, std::string_view(message));
	}

	void Server::Worker::removeClient(int client) {
		remove(server.getBufferEvent(server.getDescriptor(client)));
	}

	void Server::Worker::remove(bufferevent *buffer_event) {
		int descriptor;
		{
			auto descriptors_lock = server.lockDescriptors();
			descriptor = server.bufferEventDescriptors.at(buffer_event);
			server.bufferEventDescriptors.erase(buffer_event);
			server.bufferEvents.erase(descriptor);
		}
		bufferevent_free(buffer_event);
		{
			auto client_lock = server.lockClients();
			const int client_id = server.clients.at(descriptor);
			INFO("Removing d=" << descriptor << ", c=" << client_id);
			server.allClients.erase(client_id);
			server.freePool.insert(client_id);
		}
		readBuffers.erase(descriptor);
		writeBuffers.erase(descriptor);
	}

	void Server::Worker::removeDescriptor(int descriptor) {
		remove(server.getBufferEvent(descriptor));
	}

	void Server::Worker::queueAccept(int new_fd) {
		{
			auto lock = lockAcceptQueue();
			acceptQueue.push_back(new_fd);
		}
		event_active(acceptEvent, 0, 0);
	}

	void Server::Worker::queueClose(int client_id) {
		queueClose(server.getBufferEvent(server.getDescriptor(client_id)));
	}

	void Server::Worker::queueClose(bufferevent *buffer_event) {
		{
			auto lock = lockCloseQueue();
			closeQueue.insert(buffer_event);
		}
	}

	void Server::run() {
		if (!threads.empty())
			throw std::runtime_error("Cannot run server: already running");

		connected = true;

		makeName();

		acceptThread = std::thread([this] {
			mainLoop();	
		});

		for (size_t i = 0; i < threadCount; ++i) {
			workers.emplace_back(makeWorker(chunkSize));
			threads.emplace_back(std::thread([i, &worker = *workers.back()] {
				worker.work(i);
			}));
		}

		for (auto &thread: threads)
			thread.join();

		acceptThread.join();
		threads.clear();
		workers.clear();
	}

	void Server::mainLoop() {
		base = event_base_new();
		if (!base)
			throw std::runtime_error("Couldn't initialize libevent: " + std::string(strerror(errno)));

		evconnlistener *listener = evconnlistener_new_bind(base, listener_cb, this,
			LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_CLOSE_ON_EXEC | LEV_OPT_THREADSAFE, -1, name, nameSize);

		if (listener == nullptr) {
			event_base_free(base);
			throw std::runtime_error("Couldn't initialize libevent listener: " + std::string(strerror(errno)));
		}

		signalEvent = evsignal_new(base, SIGINT, signal_cb, this);

		if (signalEvent == nullptr || event_add(signalEvent, nullptr) < 0) {
			event_base_free(base);
			evconnlistener_free(listener);
			throw std::runtime_error("Couldn't listen for SIGINT: " + std::string(strerror(errno)));
		}

		event_base_dispatch(base);

		evconnlistener_free(listener);
		event_free(signalEvent);
		event_base_free(base);
	}

	void Server::Worker::accept(int new_fd) {
		int new_client;

		{
			auto lock = server.lockClients();
			if (!server.freePool.empty()) {
				new_client = *server.freePool.begin();
				server.freePool.erase(new_client);
			} else
				new_client = ++server.lastClient;
			server.descriptors.emplace(new_client, new_fd);
			server.clients.erase(new_fd);
			server.clients.emplace(new_fd, new_client);
		}

		INFO("Accepting " << new_fd);
		evutil_make_socket_nonblocking(new_fd);
		bufferevent *buffer_event = bufferevent_socket_new(base, new_fd, BEV_OPT_CLOSE_ON_FREE);

		if (!buffer_event) {
			event_base_loopbreak(base);
			throw std::runtime_error("buffer_event is null");
		}

		{
			auto lock = server.lockDescriptors();
			if (server.bufferEvents.contains(new_fd))
				throw std::runtime_error("File descriptor " + std::to_string(new_fd) + " already has a bufferevent struct");
			server.bufferEvents.emplace(new_fd, buffer_event);
			server.bufferEventDescriptors.emplace(buffer_event, new_fd);
		}

		{
			auto lock = server.lockWorkerMap();
			server.workerMap.emplace(buffer_event, shared_from_this());
		}

		bufferevent_setcb(buffer_event, conn_readcb, nullptr, conn_eventcb, this);
		bufferevent_enable(buffer_event, EV_READ | EV_WRITE);

		if (server.addClient)
			server.addClient(*this, new_client);
	}

	void Server::Worker::handleEOF(bufferevent *buffer_event) {
		bool contains;
		{
			auto lock = lockCloseQueue();
			if ((contains = closeQueue.contains(buffer_event)))
				closeQueue.erase(buffer_event);
		}

		if (contains)
			remove(buffer_event);
	}

	void Server::Worker::handleRead(bufferevent *buffer_event) {
		const int descriptor = bufferevent_getfd(buffer_event);
		if (descriptor == -1)
			throw std::runtime_error("descriptor is -1 in Worker::handleRead");

		evbuffer *input = bufferevent_get_input(buffer_event);

		size_t readable = evbuffer_get_length(input);

		try {
			auto lock = lockReadBuffers();
			while (0 < readable) {
				std::string &str = readBuffers[descriptor];
				str.reserve(bufferSize);

				const int byte_count = evbuffer_remove(input, buffer.get(), std::min(bufferSize, readable));

				if (byte_count < 0) {
					throw NetError("Reading", errno);
				} else {
					auto &clients = server.clients;
					auto clients_lock = server.lockClients();
					const int client_id = clients.at(descriptor);
					auto &client = *server.allClients.at(client_id);

					if (!client.lineMode) {
						server.handleMessage(clients.at(descriptor), {buffer.get(), size_t(byte_count)});
						str.clear();
					} else if (client.maxLineSize < str.size() + size_t(byte_count)) {
						client.onMaxLineSizeExceeded();
						removeDescriptor(descriptor);
					} else {
						str.insert(str.size(), buffer.get(), size_t(byte_count));
						ssize_t index;
						size_t delimiter_size;
						bool done = false;
						std::tie(index, delimiter_size) = isMessageComplete(str);
						size_t to_erase = 0;
						std::string_view view = str;
						do {
							if (index != -1) {
								server.handleMessage(clients.at(descriptor), view.substr(0, index));
								if (clients.contains(descriptor)) {
									view.remove_prefix(index + delimiter_size);
									to_erase += index + delimiter_size;
									std::tie(index, delimiter_size) = isMessageComplete(view);
								} else done = true;
							} else done = true;
						} while (!done);
						if (to_erase != 0)
							str.erase(0, to_erase);
					}
				}

				readable = evbuffer_get_length(input);
			}
		} catch (const std::runtime_error &err) {
			ERROR(err.what());
			remove(buffer_event);
		}
	}

	void Server::stop() {
		event_base_loopbreak(base);
		for (auto &worker: workers)
			worker->stop();
	}

	std::shared_ptr<Server::Worker> Server::makeWorker(size_t buffer_size) {
		return std::make_shared<Server::Worker>(*this, buffer_size);
	}

	bool Server::remove(bufferevent *buffer_event) {
		std::shared_ptr<Worker> worker;
		{
			auto lock = lockWorkerMap();
			if (!workerMap.contains(buffer_event))
				return false;
			worker = workerMap.at(buffer_event);
		}
		worker->remove(buffer_event);
		return true;
	}

	bool Server::removeClient(int client) {
		int descriptor;
		{
			auto lock = lockClients();
			descriptor = descriptors.at(client);
		}

		bufferevent *buffer_event;
		{
			auto lock = lockDescriptors();
			buffer_event = bufferEvents.at(descriptor);
		}

		return remove(buffer_event);
	}

	bool Server::close(int client_id) {
		bufferevent *buffer_event;
		try {
			buffer_event = getBufferEvent(getDescriptor(client_id));
		} catch (const std::out_of_range &) {
			return false;
		}
		std::shared_ptr<Worker> worker;
		{
			auto lock = lockWorkerMap();
			worker = workerMap.at(buffer_event);
		}
		worker->queueClose(client_id);
		return true;
	}

	bool Server::close(GenericClient &client) {
		return close(client.id);
	}

	std::pair<ssize_t, size_t> Server::isMessageComplete(std::string_view view) {
		const size_t found = view.find('\n');
		return found == std::string::npos? std::pair<ssize_t, size_t>(-1, 0) : std::pair<ssize_t, size_t>(found, 1);
	}

	void listener_cb(evconnlistener *, evutil_socket_t fd, sockaddr *, int, void *data) {
		auto *server = reinterpret_cast<Server *>(data);
		server->workers.at(server->threadCursor)->queueAccept(fd);
		server->threadCursor = (server->threadCursor + 1) % server->threadCount;
	}

	void conn_readcb(bufferevent *buffer_event, void *data) {
		auto *worker = reinterpret_cast<Server::Worker *>(data);
		worker->handleRead(buffer_event);
	}

	void conn_eventcb(bufferevent *buffer_event, short events, void *data) {
		if (events & BEV_EVENT_EOF)
			reinterpret_cast<Server::Worker *>(data)->handleEOF(buffer_event);
		else if ((events & (BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT)) != 0)
			reinterpret_cast<Server::Worker *>(data)->server.remove(buffer_event);
	}

	void signal_cb(evutil_socket_t, short, void *data) {
		auto *server = reinterpret_cast<Server *>(data);
		WARN("Received SIGINT.");
		server->stop();
	}

	void worker_acceptcb(evutil_socket_t, short, void *data) {
		auto *worker = reinterpret_cast<Server::Worker *>(data);
		auto lock = worker->lockAcceptQueue();
		while (!worker->acceptQueue.empty()) {
			const int fd = worker->acceptQueue.back();
			worker->acceptQueue.pop_back();
			worker->accept(fd);
		}
	}
}
