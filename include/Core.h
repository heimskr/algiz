#pragma once

#include <memory>
#include <vector>

#include "nlohmann/json.hpp"

namespace Algiz {
	class ApplicationServer;

	constexpr size_t DEFAULT_THREAD_COUNT = 8;

	std::vector<ApplicationServer *> run(nlohmann::json &);
}
