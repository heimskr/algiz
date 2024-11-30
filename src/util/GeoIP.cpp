#include "util/GeoIP.h"

namespace Algiz {
	std::optional<GeoIP> GeoIP::singleton;

	GeoIP::GeoIP():
		valid(false) {}

	GeoIP & GeoIP::get() {
		if (!singleton) {
			singleton.emplace();
		}

		return *singleton;
	}

	GeoIP & GeoIP::get(const std::string &database_path) {
		if (!singleton || !singleton->valid) {
			singleton.emplace(database_path);
		}

		return *singleton;
	}

#ifdef ENABLE_GEOIP

	GeoIP::GeoIP(const std::string &database_path):
		valid(true),
		db(std::in_place_t{}, database_path) {}

	std::string GeoIP::getCountry(const std::string &ip) {
		std::unique_lock lock(mutex);
		auto iter = results.find(ip);

		if (iter == results.end()) {
			if (results.size() > 4096) {
				results.clear();
			}

			iter = results.emplace(ip, db->lookup_raw(ip)).first;
		}

		return db->get_field(&iter->second, "en", {"country", "names"});
	}

#else

	GeoIP::GeoIP(const std::string &):
		valid(true),
		GeoIP() {}

	std::string GeoIP::getCountry(const std::string &) {
		return "Unknown";
	}

#endif
}
