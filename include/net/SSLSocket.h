#pragma once

#include <cerrno>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <resolv.h>
#include <string.h>
#include <unistd.h>

#include "net/Socket.h"

namespace Algiz {
	class SSLSocket: public Socket {
		public:
			using Socket::Socket;
			void connect() override;
			ssize_t send(const void *, size_t) override;
			ssize_t recv(void *, size_t) override;

		private:
			SSL_CTX *sslContext = nullptr;
			SSL *ssl = nullptr;

			void connectSSL();
	};
}
