#pragma once

#include <filesystem>

namespace Algiz {
	bool isSubpath(const std::filesystem::path &base, std::filesystem::path to_check);
	std::string readFile(const std::filesystem::path &);
	std::string readFile(const std::filesystem::path &, size_t offset, size_t byte_count);
}
