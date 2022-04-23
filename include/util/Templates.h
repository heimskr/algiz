#pragma once

#include <string>

#include "nlohmann/json.hpp"

namespace Algiz {
	std::string renderTemplate(const std::string &, nlohmann::json = {});
}
