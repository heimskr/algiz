#pragma once

#include <string>

#include "nlohmann/json.hpp"

namespace Algiz {
	std::string renderTemplate(std::string_view, nlohmann::json = {});
}
