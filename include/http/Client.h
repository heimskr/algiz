#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>

#include "http/Request.h"
#include "net/GenericClient.h"

namespace Algiz::HTTP {
	class Server;

	class Client: public GenericClient {
		private:
			HTTP::Server &server;

			void handleRequest();

		public:
			Client() = delete;
			Client(HTTP::Server &server_, int id_): GenericClient(id_, true), server(server_) {}
			Client(const Client &) = delete;
			Client(Client &&) = delete;

			~Client() = default;

			Client & operator=(const Client &) = delete;
			Client & operator=(Client &&) = delete;

			Request request;

			void send(const std::string &);
			void handleInput(const std::string &message) override;

			static std::unordered_set<std::string> supportedMethods;
	};
}
