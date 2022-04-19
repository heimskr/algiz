#pragma once

#include <filesystem>

#include "net/Server.h"
#include "nlohmann/json.hpp"
#include "plugins/PluginHost.h"
#include "Options.h"

namespace Algiz::HTTP {
	class Client;

	class Server: public Algiz::Server, public Plugins::PluginHost {
		private:
			std::filesystem::path getWebRoot(const nlohmann::json &) const;

		protected:
			std::filesystem::path webRoot;

			void addClient(int) override;

		public:
			Server(const Options &);

			void handleGet(HTTP::Client &, const std::string &path);
	};
}
