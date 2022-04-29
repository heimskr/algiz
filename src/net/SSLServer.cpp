#include <iostream>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http/Client.h"
#include "net/NetError.h"
#include "net/SSLServer.h"
#include "Log.h"

namespace Algiz {
	SSLServer::SSLServer(int af_, const std::string &ip_, uint16_t port_, const std::string &cert,
	                     const std::string &key, size_t chunk_size):
	Server(af_, ip_, port_, chunk_size), sslContext(SSL_CTX_new(TLS_server_method())) {
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

	void SSLServer::close(int descriptor) {
		SSL *ssl = ssls.at(descriptor);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssls.erase(descriptor);
		::close(descriptor);
	}

	ssize_t SSLServer::send(int client, std::string_view message) {
		SSL *ssl = ssls.at(descriptors.at(client));
		size_t written = 0;
		if (SSL_write_ex(ssl, message.begin(), message.size(), &written) <= 0) {
			ERROR("SSLServer::send failed:");
			ERR_print_errors_fp(stderr);
		}

		return ssize_t(written);
	}

	ssize_t SSLServer::send(int client, const std::string &message) {
		return send(client, std::string_view(message));
	}

	ssize_t SSLServer::read(int descriptor, void *buffer, size_t size) {
		SSL *ssl = ssls.at(descriptor);
		size_t read_bytes;
		if (SSL_read_ex(ssl, buffer, size, &read_bytes) <= 0) {
			ERROR("SSLServer::read failed:");
			ERR_print_errors_fp(stderr);
		}

		return ssize_t(read_bytes);
	}

	void SSLServer::run() {
		struct sockaddr_in clientname;

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
			read_set = activeSet;

			const int poll_result = ::poll(read_set.data(), read_set.size(), 1000);
			if (poll_result < 0)
				throw NetError("poll()", errno);

			if ((read_set.front().revents & POLLIN) != 0) {
				::close(sock);
				SSL_CTX_free(sslContext);
				sslContext = nullptr;
				break;
			}

			// Service all the sockets with input pending.
			for (const pollfd &poll_fd: read_set) {
				if ((poll_fd.revents & POLLIN) != 0) {
					if (poll_fd.fd == sock) {
						// Connection request on original socket.
						int new_fd;
						const size_t size = sizeof(clientname);
						new_fd = ::accept(sock, (sockaddr *) &clientname, (socklen_t *) &size);
						if (new_fd < 0)
							throw NetError("accept()", errno);

						SSL *ssl = SSL_new(sslContext);
						SSL_set_fd(ssl, new_fd);

						const int flags = fcntl(new_fd, F_GETFL, 0);
						if (flags < 0) {
							ERROR("fcntl (get) failed: " << strerror(errno));
							SSL_shutdown(ssl);
							SSL_free(ssl);
							continue;
						} else if (fcntl(new_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
							ERROR("fcntl (set) failed: " << strerror(errno));
							SSL_shutdown(ssl);
							SSL_free(ssl);
						}

						if (SSL_accept(ssl) <= 0) {
							ERROR("SSL_accept failed:");
							ERR_print_errors_fp(stderr);
							SSL_shutdown(ssl);
							SSL_free(ssl);
						} else {
							activeSet.push_back(pollfd {new_fd, POLL_EVENTS, 0});
							int new_client;
							if (freePool.size() != 0) {
								new_client = *freePool.begin();
								freePool.erase(new_client);
							} else
								new_client = ++lastClient;

							descriptors.emplace(new_client, new_fd);
							ssls.emplace(new_fd, ssl);
							clients.erase(new_fd);
							clients.emplace(new_fd, new_client);
							if (addClient)
								addClient(new_client);
						}
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
}
