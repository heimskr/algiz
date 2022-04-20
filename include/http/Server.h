#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>

#include "net/Server.h"
#include "nlohmann/json.hpp"
#include "plugins/PluginHost.h"
#include "util/WeakCompare.h"
#include "Options.h"

namespace Algiz::HTTP {
	class Client;

	class Server: public Algiz::Server, public Plugins::PluginHost {
		public:
			struct HandlerArgs {
				Server &server;
				Client &client;
				const std::string path;
				const std::vector<std::string> parts;

				HandlerArgs(Server &server_, Client &client_, std::string &&path_, std::vector<std::string> &&parts_):
					server(server_), client(client_), path(std::move(path_)), parts(std::move(parts_)) {}
			};

		private:
			std::filesystem::path getWebRoot(const nlohmann::json &) const;
			bool validatePath(const std::string &) const;
			std::vector<std::string> getParts(const std::string &) const;

		protected:
			void addClient(int) override;

		public:
			Options &options;

			std::filesystem::path webRoot;
			std::list<PrePtr<HandlerArgs>> handlers;

			Server(Options &);

			void handleGet(HTTP::Client &, const std::string &path);
	};
}
