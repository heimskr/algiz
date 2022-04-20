#pragma once

#include <functional>
#include <map>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <set>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>

#include "net/Server.h"

namespace Algiz {
	class SSLServer: public Server {
		protected:
			virtual void close(int descriptor) override;

		public:
			SSLServer(int af_, const std::string &ip_, uint16_t port_, const std::string &cert, const std::string &key,
			          size_t chunk_size = 1);
			virtual ~SSLServer();

			virtual ssize_t send(int client, const std::string_view &) override;
			virtual ssize_t send(int client, const std::string &) override;
			virtual void run() override;

		private:
			SSL_CTX *sslContext = nullptr;

			/** Maps descriptors to SSL pointers. */
			std::map<int, SSL *> ssls;
	};
}
