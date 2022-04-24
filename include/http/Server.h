#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>

#include "http/Request.h"
#include "net/Server.h"
#include "nlohmann/json.hpp"
#include "plugins/PluginHost.h"
#include "util/WeakCompare.h"
#include "ApplicationServer.h"

namespace Algiz::HTTP {
	class Client;

	class Server: public ApplicationServer, public Plugins::PluginHost {
		public:
			struct HandlerArgs {
				Server &server;
				Client &client;
				Request request;
				const std::vector<std::string> parts;

				HandlerArgs(Server &server_, Client &client_, Request &&request_, std::vector<std::string> &&parts_):
					server(server_), client(client_), request(std::move(request_)), parts(std::move(parts_)) {}
			};

		private:
			std::filesystem::path getWebRoot(const std::string &) const;
			bool validatePath(const std::string_view &) const;
			std::vector<std::string> getParts(const std::string_view &) const;

		public:
			std::shared_ptr<Algiz::Server> server;
			nlohmann::json options;
			std::filesystem::path webRoot;
			std::list<PrePtr<HandlerArgs>> handlers;

			Server(const std::shared_ptr<Algiz::Server> &, const nlohmann::json &options_);
			~Server();

			void run() override;
			void stop() override;
			void handleGet(HTTP::Client &, const Request &);
			void send400(HTTP::Client &);
			void send403(HTTP::Client &);
	};
}
