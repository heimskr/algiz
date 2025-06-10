#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/CountryFilter.h"
#include "util/FS.h"
#include "util/GeoIP.h"
#include "util/Util.h"

#include "Log.h"

namespace Algiz::Plugins {
	void CountryFilter::postinit(PluginHost *host) {
		auto &http = dynamic_cast<HTTP::Server &>(*(parent = host));
		http.server->ipFilters.emplace_back(filterer);

		if (auto iter = config.find("whitelist"); iter != config.end()) {
			whitelist = iter->get<std::set<std::string>>();
		}

		if (auto iter = config.find("blacklist"); iter != config.end()) {
			blacklist = iter->get<std::set<std::string>>();
		}

		if (auto iter = config.find("message"); iter != config.end()) {
			message = iter->get<std::string>();
		}
	}

	void CountryFilter::cleanup(PluginHost *host) {
		auto &http = dynamic_cast<HTTP::Server &>(*host);
		PluginHost::erase(http.server->ipFilters, filterer);
	}

	bool CountryFilter::filter(const std::string &ip, int fd) {
		if (GeoIP &geo = GeoIP::get()) {
			std::string country = "Unknown";

			try {
				country = geo.getCountry(ip);
			} catch (const std::exception &) {}

			if ((!whitelist || whitelist->contains(country)) && (!blacklist || !blacklist->contains(country))) {
				return true;
			}

			INFO("Denying request from " << country << '.');

			if (message) {
				(void) ::write(fd, message->c_str(), message->size());
			}

			return false;
		}

		return true;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::CountryFilter;
}
