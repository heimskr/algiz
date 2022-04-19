#pragma once

#include <filesystem>

#include "net/Server.h"
#include "plugins/PluginHost.h"
#include "Options.h"

namespace Algiz::HTTP {
	class Client;

	class Server: public Algiz::Server, public Plugins::PluginHost {
		protected:
			std::filesystem::path webRoot;

			void addClient(int) override;

		public:
			Server(const Options &);

			void handleGet(HTTP::Client &, const std::string &path);
	};
}
