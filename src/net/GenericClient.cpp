#include "net/GenericClient.h"
#include "util/GeoIP.h"

#include <format>

namespace Algiz {
	std::string GenericClient::describe() {
		GeoIP &geoip = GeoIP::get();

		if (!geoip.isValid()) {
			return ip;
		}

		return std::format("{} ({})", ip, geoip.getCountry(ip));
	}
}
