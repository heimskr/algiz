#include <fcntl.h>
#include <iostream>
#include <stdexcept>

#include "net/NetError.h"
#include "net/SSLSocket.h"

#include "Log.h"

namespace Algiz {
	void SSLSocket::connect() {
		Socket::connect();
		connectSSL();
	}

	ssize_t SSLSocket::send(const void *data, size_t bytes) {
		if (!connected)
			throw std::invalid_argument("Socket not connected");
		size_t written = 0;
		const int status = SSL_write_ex(ssl, data, bytes, &written);
		if (status == 1) {
			SPAM("SSLSocket::send(status == 1): bytes[" << bytes << "], written[" << written << "]");
			std::string str = static_cast<const char *>(data);
			while (!str.empty() && (str.back() == '\r' || str.back() == '\n'))
				str.pop_back();
			SPAM("    \"" << str << "\"");
			return static_cast<ssize_t>(written);
		}

		SPAM("SSLSocket::send(status == " << status << "): bytes[" << bytes << "], written[" << written << "], error["
		    << SSL_get_error(ssl, status) << "], errno[" << errno << "]");
		return -SSL_get_error(ssl, status);
	}

	ssize_t SSLSocket::recv(void *data, size_t bytes) {
		if (!connected)
			throw std::invalid_argument("Socket not connected");

		fd_set fds_copy = fds;
		ssize_t status = select(FD_SETSIZE, &fds_copy, nullptr, nullptr, nullptr);

		if (status < 0) {
			char error[64] = "?";
			strerror_r(status, error, sizeof(error));
			SPAM("select status: " << error);
			throw NetError(errno);
		}
			
		if (FD_ISSET(netFD, &fds_copy)) {
			bool read_blocked = false;
			size_t bytes_read = 0;
			size_t total_bytes_read = 0;

			do {
				read_blocked = false;
				status = SSL_read_ex(ssl, data, bytes, &bytes_read);

				total_bytes_read += bytes_read;
				if (bytes <= bytes_read)
					bytes = 0;
				else
					bytes -= bytes_read;

				if (status == 0)
					switch (SSL_get_error(ssl, status)) {
						case SSL_ERROR_NONE:
							SPAM("SSL_ERROR_NONE");
							return ssize_t(bytes_read);
							break;
						
						case SSL_ERROR_ZERO_RETURN:
							SPAM("SSL_ERROR_ZERO_RETURN");
							close();
							break;
						
						case SSL_ERROR_WANT_READ:
							SPAM("SSL_ERROR_WANT_READ");
							read_blocked = true;
							break;
						
						case SSL_ERROR_WANT_WRITE:
							SPAM("SSL_ERROR_WANT_WRITE");
							break;
						
						case SSL_ERROR_SYSCALL:
							SPAM("SSL_ERROR_SYSCALL");
							close();
							break;
		
						default:
							SPAM("default");
							close();
							break;
					}
				else {
					std::string read_str(static_cast<const char *>(data), bytes_read);
					while (!read_str.empty() && (read_str.back() == '\r' || read_str.back() == '\n'))
						read_str.pop_back();
					SPAM("SSLSocket::recv(status == 1): \"" << read_str << "\"");
				}
			} while (SSL_pending(ssl) != 0 && !read_blocked && 0 < bytes);

			return ssize_t(total_bytes_read);
		}

		if (FD_ISSET(controlRead, &fds_copy)) {
			ControlMessage message = ControlMessage::None;
			status = ::read(controlRead, &message, 1);
			if (status < 0) {
				char error[64] = "?";
				strerror_r(errno, error, sizeof(error));
				SPAM("control_fd status: " << error);
				throw NetError(errno);
			}

			if (message != ControlMessage::Close)
				SPAM("Unknown control message: '" << static_cast<char>(message) << "'");

			SSL_free(ssl);
			::close(netFD);
			SSL_CTX_free(sslContext);
			return 0;
		}

		SPAM("No file descriptor is ready.");
		return -1;
	}

	void SSLSocket::connectSSL() {
		const SSL_METHOD *method = TLS_client_method();
		sslContext = SSL_CTX_new(method);

		if (sslContext == nullptr)
			throw std::runtime_error("SSLSocket::connectSSL failed");

		ssl = SSL_new(sslContext);
		if (ssl == nullptr)
			throw std::runtime_error("SSLSocket::connectSSL: SSL_new failed");

		SSL_set_fd(ssl, netFD);

		int status = SSL_connect(ssl);
		if (status != 1) {
			int error = SSL_get_error(ssl, status);
			if (error != SSL_ERROR_WANT_READ)
				throw std::runtime_error("SSLSocket::connectSSL: SSL_connect failed ("
					+ std::to_string(SSL_get_error(ssl, status)) + ")");
		}

		int flags = fcntl(netFD, F_GETFL, 0);
		if (flags < 0)
			throw std::runtime_error("fcntl(F_GETFL) returned " + std::to_string(flags));
		flags |= O_NONBLOCK;
		status = fcntl(netFD, F_SETFL, flags);
		if (status < 0)
			throw std::runtime_error("fcntl(F_SETFL) returned " + std::to_string(status));

		SPAM("Connected with " << SSL_get_cipher(ssl));

		X509 *cert = SSL_get_peer_certificate(ssl);
		if (cert != nullptr) {
			SPAM("Server");
			char *line = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
			SPAM("Subject: " << line);
			free(line);
			line = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);
			SPAM("Issuer: " << line);
			free(line);
			X509_free(cert);
		} else
			SPAM("No client certificates configured.");
	}
}
