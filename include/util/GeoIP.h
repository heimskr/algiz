#pragma once

#include "../../config.h"

#ifdef ENABLE_GEOIP
#include "GeoLite2PP.hpp"
#endif

#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace Algiz {
	class GeoIP {
		public:
			GeoIP();
			GeoIP(const std::string &database_path);

			std::string getCountry(const std::string &ip);

			static GeoIP & get(const std::string &database_path);
			static GeoIP & get();

#ifdef ENABLE_GEOIP
			inline bool isValid() const { return valid; }
#else
			inline bool isValid() const { return false; }
#endif

			inline operator bool() const { return isValid(); }

		private:
			bool valid;

#ifdef ENABLE_GEOIP
			std::mutex mutex;
			std::optional<GeoLite2PP::DB> db;
			std::map<std::string, MMDB_lookup_result_s> results;
#endif

			static std::optional<GeoIP> singleton;
	};
}
