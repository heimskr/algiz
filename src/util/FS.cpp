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
		std::string out;
		if (!stream.is_open())
			throw std::runtime_error("Couldn't open file for reading");
		stream.seekg(0, std::ios::end);
		out.reserve(stream.tellg());
		stream.seekg(0, std::ios::beg);
		out.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		stream.close();
		return out;
	}
}