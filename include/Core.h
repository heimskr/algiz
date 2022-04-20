#pragma once

#include <memory>
#include <vector>

#include "nlohmann/json.hpp"

namespace Algiz {
	class ApplicationServer;

	std::vector<ApplicationServer *> run(nlohmann::json &);
}
