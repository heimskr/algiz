#pragma once

#include <functional>
#include <map>
#include <string>
#include <unordered_set>

#include "net/GenericClient.h"

namespace Algiz::HTTP {
	class Server;

	class Client: public GenericClient {
		private:
			HTTP::Server &server;

			void handleRequest();

		public:
			enum class Mode {Method, Headers, Content};

			Mode mode = Mode::Method;
			std::map<std::string, std::string> headers;
			std::string content;
			std::string method;
			std::string path;

			Client(HTTP::Server &server_, int id_): GenericClient(id_, true), server(server_) {}
			Client(const Client &) = delete;
			Client(Client &&) = delete;

			Client & operator=(const Client &) = delete;
			Client & operator=(Client &&) = delete;

			void send(const std::string &);
			void handleInput(const std::string &) override;

			static std::unordered_set<std::string> supportedMethods;
	};
}
