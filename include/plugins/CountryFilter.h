#pragma once

#include "http/Server.h"
#include "plugins/Plugin.h"
#include "util/Util.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

namespace Algiz::HTTP {
	class Client;
	class Server;
}

namespace Algiz::Plugins {
	class CountryFilter: public Plugin {
		public:
			[[nodiscard]] std::string getName()        const override { return "Country Filter"; }
			[[nodiscard]] std::string getDescription() const override {
				return "Allows whitelisting/blacklisting IPs by country.";
			}
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<std::function<bool(const std::string &, int)>> filterer =
				std::make_shared<std::function<bool(const std::string &, int)>>(bind(*this, &CountryFilter::filter));

			std::optional<std::set<std::string>> whitelist;
			std::optional<std::set<std::string>> blacklist;
			std::optional<std::string> message;

		private:
			bool filter(const std::string &ip, int fd);
	};
}
