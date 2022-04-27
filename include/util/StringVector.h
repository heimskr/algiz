#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace Algiz {
	struct StringVector: public std::vector<std::string> {
		using std::vector<std::string>::vector;
		StringVector(const std::vector<std::string> &strings): std::vector<std::string>(strings) {}
		StringVector(std::vector<std::string> &&strings): std::vector<std::string>(strings) {}
		bool operator==(std::initializer_list<const char *>) const;
	};
}
