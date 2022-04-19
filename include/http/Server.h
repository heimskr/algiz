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

				HandlerArgs(Server &server_, Client &client_, const std::string &path_):
					server(server_), client(client_), path(path_) {}
			};

		private:
			std::filesystem::path getWebRoot(const nlohmann::json &) const;
			bool validatePath(const std::string &) const;

		protected:
			void addClient(int) override;

		public:
			std::filesystem::path webRoot;
			WeakSet<PreFn<HandlerArgs>> handlers;

			Server(const Options &);

			void handleGet(HTTP::Client &, const std::string &path);
	};
}
