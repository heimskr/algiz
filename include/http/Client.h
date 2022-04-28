#pragma once

#include <any>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>

#include "http/Request.h"
#include "net/GenericClient.h"
#include "util/StringVector.h"

namespace Algiz::HTTP {
	class Server;

	class Client: public GenericClient {
		private:
			HTTP::Server &server;

			void handleRequest();

		public:
			Request request;
			std::map<std::string, std::any> session;
			bool isWebSocket = false;
			StringVector webSocketPath;

			Client() = delete;
			Client(HTTP::Server &server_, int id_): GenericClient(id_, true, 8192), server(server_) {}
			Client(const Client &) = delete;
			Client(Client &&) = delete;

			~Client() = default;

			Client & operator=(const Client &) = delete;
			Client & operator=(Client &&) = delete;

			void send(const std::string &);
			void handleInput(const std::string &message) override;
			void onMaxLineSizeExceeded() override;

			static std::unordered_set<std::string> supportedMethods;
	};
}
