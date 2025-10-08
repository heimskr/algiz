#pragma once

#include <chrono>
#include <filesystem>

struct stat;

namespace Algiz {
	bool isSubpath(const std::filesystem::path &base, std::filesystem::path to_check);
	std::string readFile(const std::filesystem::path &);
	std::string readFile(const std::filesystem::path &, size_t offset, size_t byte_count);
	std::time_t lastWritten(const std::filesystem::path &);
	bool canExecute(const std::filesystem::path &);
	struct stat getInfo(const std::filesystem::path &);
	bool selfOwns(const std::filesystem::path &);
	bool selfOwns(const struct stat &);
	bool groupOwns(const std::filesystem::path &);
	bool groupOwns(const struct stat &);
	bool isNewerThan(const std::filesystem::path &, const std::filesystem::path &);
}
