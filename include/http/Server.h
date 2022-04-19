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
				const std::filesystem::path path;

				HandlerArgs(Server &server_, Client &client_, const std::filesystem::path &path_):
					server(server_), client(client_), path(path_) {}
			};

		private:
			std::filesystem::path getWebRoot(const nlohmann::json &) const;

		protected:
			std::filesystem::path webRoot;

			void addClient(int) override;

		public:
			Server(const Options &);

			WeakSet<PreFn<HandlerArgs>> handlers;

			void handleGet(HTTP::Client &, const std::string &path);
	};
}
