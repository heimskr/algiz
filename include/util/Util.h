#pragma once

#include <string>

namespace Algiz {
	long parseLong(const std::string &, int base = 10);
	long parseLong(const char *, int base = 10);
	unsigned long parseUlong(const std::string &, int base = 10);
	unsigned long parseUlong(const char *, int base = 10);
	std::string toLower(std::string);
	std::string toUpper(std::string);
}
