#pragma once

#include "net/Server.h"
#include "net/Certificates.h"
#include "threading/Lockable.h"

#include <functional>
#include <map>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <set>
#include <shared_mutex>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>

namespace Algiz {
	class SSLServer: public Server {
		public:
			SSLServer(Core &core, int af, std::string ip, uint16_t port, const std::string &cert, const std::string &key, const std::string &chain, size_t threadCount, size_t chunkSize = 1);
			SSLServer(const SSLServer &) = delete;
			SSLServer(SSLServer &&) = delete;

			~SSLServer() override;

			SSLServer & operator=(const SSLServer &) = delete;
			SSLServer & operator=(SSLServer &&) = delete;

		public:
			SSL_CTX *sslContext = nullptr;
			std::mutex sslContextMutex;

			/** Maps descriptors to SSL pointers. */
			std::map<int, SSL *> ssls;
			std::mutex sslsMutex;

			/** Maps descriptors to mutexes that need to be locked when using an SSL object. */
			std::map<int, std::mutex> sslMutexes;

			std::unordered_map<std::string, Certificates> certificatesMap;
			std::shared_mutex certificatesMutex;

			Lockable<std::function<void(const char *)>> requestCertificate;

			void addCertificate(std::string hostname, const std::string &certificate, const std::string &private_key, const std::string &rest_of_chain);

			std::shared_ptr<Worker> makeWorker(size_t buffer_size, size_t id) override;

			class Worker: public Server::Worker {
				public:
					using Server::Worker::Worker;

					void remove(bufferevent *) override;
					void accept(int new_fd) override;
			};

			friend class Worker;
	};
}
