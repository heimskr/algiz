#include <iostream>

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <event2/bufferevent_ssl.h>

#include "http/Client.h"
#include "net/NetError.h"
#include "net/SSLServer.h"

#include "Log.h"

namespace Algiz {
	SSLServer::SSLServer(int af_, const std::string &ip_, uint16_t port_, const std::string &cert,
							const std::string &key, size_t thread_count, size_t chunk_size):
	Server(af_, ip_, port_, thread_count, chunk_size), sslContext(SSL_CTX_new(TLS_server_method())) {
		if (sslContext == nullptr) {
			perror("Unable to create SSL context");
			ERR_print_errors_fp(stderr);
			throw std::runtime_error("SSLServer OpenSSL context initialization failed");
		}

		if (SSL_CTX_use_certificate_file(sslContext, cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
			ERR_print_errors_fp(stderr);
			SSL_CTX_free(sslContext);
			throw std::runtime_error("SSLServer OpenSSL certificate initialization failed");
		}

		if (SSL_CTX_use_PrivateKey_file(sslContext, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
			ERR_print_errors_fp(stderr);
			SSL_CTX_free(sslContext);
			throw std::runtime_error("SSLServer OpenSSL private key initialization failed");
		}
	}

	SSLServer::~SSLServer() {
		if (sslContext != nullptr) {
			SSL_CTX_free(sslContext);
			sslContext = nullptr;
		}
	}

	void SSLServer::Worker::remove(bufferevent *buffer_event) {
		int descriptor = -1;
		{
			auto descriptors_lock = server.lockDescriptors();
			descriptor = server.bufferEventDescriptors.at(buffer_event);
			server.bufferEventDescriptors.erase(buffer_event);
			server.bufferEvents.erase(descriptor);
		}
		{
			auto client_lock = server.lockClients();
			const int client_id = server.clients.at(descriptor);
			server.allClients.erase(client_id);
			server.freePool.insert(client_id);
			server.descriptors.erase(client_id);
			server.clients.erase(descriptor);
		}
		{
			auto read_lock = lockReadBuffers();
			readBuffers.erase(descriptor);
		}
		{
			auto worker_lock = server.lockWorkerMap();
			server.workerMap.erase(buffer_event);
		}
		{
			auto &ssl_server = dynamic_cast<SSLServer &>(server);
			auto ssls_lock = std::unique_lock(ssl_server.sslsMutex);
			ssl_server.ssls.erase(descriptor);
		}
		bufferevent_free(buffer_event);
	}

	void SSLServer::Worker::accept(int new_fd) {
		auto &ssl_server = dynamic_cast<SSLServer &>(server);

		SSL *ssl = nullptr;

		{
			auto context_lock = std::unique_lock(ssl_server.sslContextMutex);
			ssl = SSL_new(ssl_server.sslContext);
		}

		if (ssl == nullptr)
			throw std::runtime_error("ssl is null");

		int new_client = -1;

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

		evutil_make_socket_nonblocking(new_fd);

		bufferevent *buffer_event = bufferevent_openssl_socket_new(base, new_fd, ssl, BUFFEREVENT_SSL_ACCEPTING,
			BEV_OPT_CLOSE_ON_FREE);

		if (buffer_event == nullptr) {
			event_base_loopbreak(base);
			throw std::runtime_error("buffer_event is null");
		}

		{
			auto lock = server.lockDescriptors();
			if (server.bufferEvents.contains(new_fd))
				throw std::runtime_error("File descriptor " + std::to_string(new_fd) + " already has a bufferevent "
					"struct");
			server.bufferEvents.emplace(new_fd, buffer_event);
			server.bufferEventDescriptors.emplace(buffer_event, new_fd);
		}

		{
			auto lock = std::unique_lock(ssl_server.sslsMutex);
			ssl_server.ssls.emplace(new_fd, ssl);
			ssl_server.sslMutexes.try_emplace(new_fd);
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

	std::shared_ptr<Server::Worker> SSLServer::makeWorker(size_t buffer_size, size_t id) {
		return std::make_shared<SSLServer::Worker>(*this, buffer_size, id);
	}
}
