#include "http/Client.h"
#include "net/NetError.h"
#include "net/SSLServer.h"
#include "util/Defer.h"
#include "util/GeoIP.h"
#include "Log.h"

#include <event2/bufferevent_ssl.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Algiz {
	SSLServer::SSLServer(Core &core, int af, std::string ip, uint16_t port, const std::string &cert, const std::string &key, const std::string &chain, size_t threadCount, size_t chunkSize):
		Server(core, af, ip, port, threadCount, chunkSize),
		sslContext(SSL_CTX_new(TLS_server_method())) {
			Defer cleanup{[&] {
				ERR_print_errors_fp(stderr);
				SSL_CTX_free(sslContext);
			}};

			if (sslContext == nullptr) {
				perror("Unable to create SSL context");
				throw std::runtime_error("SSLServer OpenSSL context initialization failed");
			}

			if (!cert.empty() && SSL_CTX_use_certificate_file(sslContext, cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
				throw std::runtime_error("SSLServer OpenSSL certificate initialization failed");
			}

			if (!key.empty() && SSL_CTX_use_PrivateKey_file(sslContext, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
				throw std::runtime_error("SSLServer OpenSSL private key initialization failed");
			}

			if (!chain.empty() && SSL_CTX_use_certificate_chain_file(sslContext, chain.c_str()) <= 0) {
				throw std::runtime_error("SSLServer OpenSSL certificate chain initialization failed");
			}

			SSL_CTX_set_tlsext_servername_arg(sslContext, this);
			SSL_CTX_set_tlsext_servername_callback(sslContext, (+[](SSL *ssl, int *, void *arg) {
				const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
				if (servername == nullptr) {
					return SSL_TLSEXT_ERR_OK;
				}

				auto *server = reinterpret_cast<SSLServer *>(arg);
				std::shared_lock lock{server->certificatesMutex};

				if (auto iter = server->certificatesMap.find(servername); iter != server->certificatesMap.end()) {
					const auto &[certificate, private_key, chain] = iter->second;

					if (SSL_use_certificate(ssl, certificate) != 1) {
						ERROR("Failed to set certificate for hostname " << servername);
						return SSL_TLSEXT_ERR_ALERT_FATAL;
					}

					for (size_t i = 0; X509 *intermediate: chain) {
						++i;
						if (SSL_add1_chain_cert(ssl, intermediate) != 1) {
							ERROR("Failed to add intermediate " << i << " for hostname " << servername);
							return SSL_TLSEXT_ERR_ALERT_FATAL;
						}
					}

					if (SSL_use_PrivateKey(ssl, private_key) != 1) {
						ERROR("Failed to set private key for hostname " << servername);
						return SSL_TLSEXT_ERR_ALERT_FATAL;
					}
				} else {
					auto lock = server->requestCertificate.sharedLock();
					if (server->requestCertificate) {
						server->requestCertificate(servername);
					} else {
						WARN("Can't request certificate for " << servername << "!");
					}
				}

				return SSL_TLSEXT_ERR_OK;
			}));

			cleanup.release();
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
			if (server.closeHandler) {
				server.closeHandler(client_id);
			}
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
		// Sometimes this is called during ~Server after ~SSLServer, in which case the dynamic_cast will return nullptr.
		if (auto *ssl_server = dynamic_cast<SSLServer *>(&server)) {
			auto ssls_lock = std::unique_lock(ssl_server->sslsMutex);
			ssl_server->ssls.erase(descriptor);
		}
		bufferevent_free(buffer_event);
	}

	void SSLServer::Worker::accept(int new_fd) {
		auto &ssl_server = dynamic_cast<SSLServer &>(server);

		std::string ip;
		sockaddr_in6 addr6 {};
		socklen_t addr6_len = sizeof(addr6);
		if (getpeername(new_fd, reinterpret_cast<sockaddr *>(&addr6), &addr6_len) == 0) {
			char ip_buffer[INET6_ADDRSTRLEN];
			if (inet_ntop(addr6.sin6_family, &addr6.sin6_addr, ip_buffer, sizeof(ip_buffer)) != nullptr) {
				ip = ip_buffer;
			} else {
				WARN("inet_ntop failed: " << strerror(errno));
			}
		}

		if (std::string_view(ip).substr(0, 7) == "::ffff:" && ip.find('.') != std::string::npos) {
			ip.erase(0, 7);
		}

		if (!server.ipFilters.empty()) {
			for (const auto &weak_filter: server.ipFilters) {
				if (auto filter = weak_filter.lock()) {
					if (!(*filter)(ip, new_fd)) {
						::close(new_fd);
						return;
					}
				}
			}
		}

		SSL *ssl = nullptr;

		{
			auto context_lock = std::unique_lock(ssl_server.sslContextMutex);
			ssl = SSL_new(ssl_server.sslContext);
		}

		if (ssl == nullptr) {
			throw std::runtime_error("ssl is null");
		}

		int new_client = -1;

		{
			auto lock = server.lockClients();
			if (!server.freePool.empty()) {
				new_client = *server.freePool.begin();
				server.freePool.erase(new_client);
			} else {
				new_client = ++server.lastClient;
			}
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
			if (server.bufferEvents.contains(new_fd)) {
				throw std::runtime_error(std::format("File descriptor {} already has a bufferevent struct", new_fd));
			}
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

		if (server.addClient) {
			auto lock = server.lockClients();
			server.addClient(*this, new_client, ip);
		}

		bufferevent_setcb(buffer_event, conn_readcb, conn_writecb, conn_eventcb, this);
		bufferevent_enable(buffer_event, EV_READ | EV_WRITE);
	}

	void SSLServer::addCertificate(std::string hostname, const std::string &certificate, const std::string &private_key, const std::string &rest_of_chain) {
		std::unique_lock lock{certificatesMutex};

		if (auto iter = certificatesMap.find(hostname); iter != certificatesMap.end()) {
			certificatesMap.erase(iter);
		}

		std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()), BIO_free);
		if (bio == nullptr) {
			throw std::runtime_error("Couldn't create BIO");
		}

		if (BIO_puts(bio.get(), certificate.c_str()) <= 0) {
			throw std::runtime_error("Couldn't read certificate");
		}

		X509 *x509 = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);

		if (BIO_puts(bio.get(), rest_of_chain.c_str()) <= 0) {
			throw std::runtime_error("Couldn't read rest of chain");
		}

		std::vector<X509 *> chain;

		X509 *intermediate = nullptr;
		while ((intermediate = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr)) != nullptr) {
			chain.push_back(intermediate);
		}

		if (BIO_puts(bio.get(), private_key.c_str()) <= 0) {
			X509_free(x509);
			throw std::runtime_error("Couldn't read private key");
		}

		EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);

		certificatesMap.try_emplace(std::move(hostname), x509, pkey, std::move(chain));
	}

	std::shared_ptr<Server::Worker> SSLServer::makeWorker(size_t buffer_size, size_t id) {
		return std::make_shared<SSLServer::Worker>(*this, buffer_size, id);
	}
}
