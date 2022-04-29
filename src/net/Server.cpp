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
	Server::Server(int af_, const std::string &ip_, uint16_t port_, size_t chunk_size):
		af(af_), ip(ip_), port(port_), chunkSize(chunk_size), buffer(new char[chunk_size]) {}

	Server::~Server() {
		stop();
		delete[] buffer;
	}

	int Server::makeSocket(bool reusable) {
		int sock = ::socket(af, SOCK_STREAM, 0);
		if (sock < 0)
			throw NetError("::socket", errno);

		if (reusable) {
			int one = 1;
			if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) != 0)
				throw NetError("setsockopt", errno);
		}

		sockaddr *name = nullptr;
		size_t name_size = 0;

		sockaddr_in  name4 {.sin_family  = AF_INET,  .sin_port  = htons(port)};
		sockaddr_in6 name6 {.sin6_family = AF_INET6, .sin6_port = htons(port)};

		int status = 0;

		if (af == AF_INET) {
			name = reinterpret_cast<sockaddr *>(&name4);
			name_size = sizeof(name4);
			status = inet_pton(AF_INET, ip.c_str(), &name4.sin_addr.s_addr);
		} else if (af == AF_INET6) {
			name = reinterpret_cast<sockaddr *>(&name6);
			name_size = sizeof(name6);
			status = inet_pton(AF_INET6, ip.c_str(), &name6.sin6_addr);
		} else
			throw std::invalid_argument("Unsupported or invalid address family: " + std::to_string(af));

		if (status != 1)
			throw std::invalid_argument("Couldn't parse IP address \"" + ip + "\"");

		if (::bind(sock, name, name_size) < 0)
			throw NetError("::bind", errno);

		return sock;
	}

	void Server::close(int descriptor) {
		::close(descriptor);
	}

	void Server::readFromClient(int descriptor) {
		std::string &str = buffers[descriptor];
		str.reserve(chunkSize);

		ssize_t byte_count = read(descriptor, buffer, chunkSize);

		if (byte_count < 0) {
			throw NetError("Reading", errno);
		} else if (byte_count == 0) {
			end(descriptor);
		} else {
			const int client_id = clients.at(descriptor);
			auto &client = *allClients.at(client_id);

			if (!client.lineMode) {
				handleMessage(clients.at(descriptor), {buffer, size_t(byte_count)});
				buffers[descriptor].clear();
			} else if (client.maxLineSize < str.size() + byte_count) {
				client.onMaxLineSizeExceeded();
				end(descriptor);
			} else {
				str.insert(str.size(), buffer, byte_count);
				ssize_t index;
				size_t delimiter_size;
				bool done = false;
				std::tie(index, delimiter_size) = isMessageComplete(str);
				do {
					if (index != -1) {
						handleMessage(clients.at(descriptor), str.substr(0, index));
						if (clients.count(descriptor) == 1) {
							str.erase(0, index + delimiter_size);
							std::tie(index, delimiter_size) = isMessageComplete(str);
						} else done = true;
					} else done = true;
				} while (!done);
			}
		}
	}

	void Server::handleMessage(int client, const std::string &message) {
		if (messageHandler)
			messageHandler(client, message);
		else
			std::cerr << "Server: got message from client " << client << ": \"" << message << "\"\n";
	}

	void Server::end(int descriptor) {
		close(descriptor);
		const int client = clients.at(descriptor);
		descriptors.erase(client);
		buffers.erase(descriptor);
		clients.erase(descriptor);
		allClients.erase(client);
		for (auto iter = activeSet.begin(), end = activeSet.end(); iter != end; ++iter)
			if (iter->fd == descriptor) {
				activeSet.erase(iter);
				break;
			}
		if (onEnd)
			onEnd(client, descriptor);
		freePool.insert(client);
	}

	ssize_t Server::send(int client, std::string_view message) {
		return ::write(descriptors.at(client), message.begin(), message.size());
	}

	ssize_t Server::send(int client, const std::string &message) {
		return send(client, std::string_view(message));
	}

	ssize_t Server::read(int descriptor, void *buffer, size_t size) {
		return ::read(descriptor, buffer, size);
	}

	void Server::removeClient(int client) {
		end(descriptors.at(client));
	}

	void Server::run() {
		struct sockaddr_in clientname;
		size_t size;

		int control_pipe[2];
		if (pipe(control_pipe) < 0)
			throw NetError("pipe()", errno);

		controlRead  = control_pipe[0];
		controlWrite = control_pipe[1];

		sock = makeSocket();
		if (::listen(sock, 1) < 0)
			throw NetError("Listening", errno);

		activeSet.clear();
		activeSet.push_back(pollfd {controlRead, POLL_EVENTS, 0});
		activeSet.push_back(pollfd {sock, POLL_EVENTS, 0});

		connected = true;

		std::vector<pollfd> read_set;
		for (;;) {
			// Block until input arrives on one or more active sockets.

			// TODO: can we just use activeSet directly?
			read_set = activeSet;

			const int poll_result = ::poll(read_set.data(), read_set.size(), 1000);
			if (poll_result < 0)
				throw NetError("poll()", errno);

			if ((read_set.front().revents & POLLIN) != 0) {
				::close(sock);
				break;
			}

			// Service all the sockets with input pending.
			for (const pollfd &poll_fd: read_set) {
				if ((poll_fd.revents & POLLIN) != 0) {
					if (poll_fd.fd == sock) {
						// Connection request on original socket.
						int new_fd;
						size = sizeof(clientname);
						new_fd = ::accept(sock, (sockaddr *) &clientname, (socklen_t *) &size);
						if (new_fd < 0)
							throw NetError("accept()", errno);
						activeSet.push_back(pollfd {new_fd, POLL_EVENTS, 0});
						int new_client;
						if (freePool.size() != 0) {
							new_client = *freePool.begin();
							freePool.erase(new_client);
						} else
							new_client = ++lastClient;
						descriptors.emplace(new_client, new_fd);
						clients.erase(new_fd);
						clients.emplace(new_fd, new_client);
						if (addClient)
							addClient(new_client);
					} else if (poll_fd.fd != controlRead) {
						// Data arriving on an already-connected socket.
						try {
							readFromClient(poll_fd.fd);
						} catch (const NetError &err) {
							std::cerr << err.what() << "\n";
							removeClient(clients.at(poll_fd.fd));
						}
					}
				}
			}
		}
	}

	void Server::stop() {
		if (connected) {
			ControlMessage message = ControlMessage::Close;
			::write(controlWrite, &message, 1);
			connected = false;
		}
	}

	std::pair<ssize_t, size_t> Server::isMessageComplete(const std::string &buf) {
		const size_t found = buf.find("\n");
		return found == std::string::npos? std::pair<ssize_t, size_t>(-1, 0) : std::pair<ssize_t, size_t>(found, 1);
	}
}
