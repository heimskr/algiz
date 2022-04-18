#pragma once

#include <filesystem>

namespace Algiz {
	bool isSubpath(const std::filesystem::path &base, std::filesystem::path to_check);
	std::string readFile(const std::filesystem::path &);
}
