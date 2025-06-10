#include "net/GenericClient.h"
#include "util/GeoIP.h"

#include <format>

namespace Algiz {
	std::string GenericClient::describe() {
		GeoIP &geoip = GeoIP::get();

		if (!geoip.isValid()) {
			return ip;
		}

		try {
			return std::format("{} ({})", ip, geoip.getCountry(ip));
		} catch (const std::exception &err) {
			return std::format("{} (?: {})", ip, err.what());
		}
	}
}
