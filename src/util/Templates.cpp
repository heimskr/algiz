#include "inja.hpp"

#include "util/Templates.h"
#include "util/Usage.h"
#include "util/Util.h"

namespace Algiz {
	std::string renderTemplate(const std::string &input, nlohmann::json json) {
		size_t virtual_memory, resident_memory;
		if (!getMemoryUsage(virtual_memory, resident_memory)) {
			json["virtual"] = "???";
			json["resident"] = "???";
		} else {
			json["virtual"] = std::to_string(virtual_memory);
			json["resident"] = std::to_string(resident_memory);
		}

		json["time"] = formatTime("%H:%M:%S");
		json["date"] = formatTime("%B %e, %Y");
		return inja::render(input, json);
	}
}
