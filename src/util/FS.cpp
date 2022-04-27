#include <fstream>
#include <iostream>

#include "util/FS.h"

namespace Algiz {
	bool isSubpath(const std::filesystem::path &base, std::filesystem::path to_check) {
		if (base == to_check)
			return true;

		const std::filesystem::path root("/");
		while (to_check != root) {
			if (to_check == base)
				return true;
			to_check = to_check.parent_path();
		}

		return false;
	}

	std::string readFile(const std::filesystem::path &path) {
		std::ifstream stream;
		stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		stream.open(path);
		stream.exceptions(std::ifstream::goodbit);
		if (!stream.is_open())
			throw std::runtime_error("Couldn't open file for reading");
		stream.seekg(0, std::ios::end);
		std::string out;
		out.reserve(stream.tellg());
		stream.seekg(0, std::ios::beg);
		out.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		stream.close();
		return out;
	}

	std::string readFile(const std::filesystem::path &path, size_t offset, size_t byte_count) {
		std::ifstream stream;
		stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		stream.open(path);
		stream.exceptions(std::ifstream::goodbit);
		if (!stream.is_open())
			throw std::runtime_error("Couldn't open file for reading");
		stream.seekg(offset, std::ios::beg);
		char *buffer = new char[byte_count];
		try {
			stream.read(buffer, byte_count);
			stream.close();
		} catch (...) {
			delete[] buffer;
			throw;
		}

		std::string out(buffer, byte_count);
		delete[] buffer;
		return out;
	}

	std::time_t lastWritten(const std::filesystem::path &path) {
		// Hideous temporary hack.
#ifdef __APPLE__
		return std::chrono::file_clock::to_time_t(std::filesystem::last_write_time(path));
#endif
		return std::chrono::system_clock::to_time_t(std::chrono::file_clock::to_sys(
			std::filesystem::last_write_time(path)));
	}
}
