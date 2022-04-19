#include "util/Util.h"

namespace Algiz {
	long parseLong(const std::string &str, int base) {
		const char *c_str = str.c_str();
		char *end = nullptr;;
		const long parsed = strtol(c_str, &end, base);
		if (c_str + str.length() != end)
			throw std::invalid_argument("Not an integer: \"" + str + "\"");
		return parsed;
	}

	long parseLong(const char *str, int base) {
		char *end = nullptr;
		const long parsed = strtol(str, &end, base);
		if (str + strlen(str) != end)
			throw std::invalid_argument("Not an integer: \"" + std::string(str) + "\"");
		return parsed;
	}

	unsigned long parseUlong(const std::string &str, int base) {
		const char *c_str = str.c_str();
		char *end = nullptr;;
		const unsigned long parsed = strtoul(c_str, &end, base);
		if (c_str + str.length() != end)
			throw std::invalid_argument("Not an integer: \"" + str + "\"");
		return parsed;
	}

	unsigned long parseUlong(const char *str, int base) {
		char *end = nullptr;
		const unsigned long parsed = strtoul(str, &end, base);
		if (str + strlen(str) != end)
			throw std::invalid_argument("Not an integer: \"" + std::string(str) + "\"");
		return parsed;
	}

	std::string toLower(std::string str) {
		for (size_t i = 0, length = str.length(); i < length; ++i)
			if ('A' <= str[i] && str[i] <= 'Z')
				str[i] += 'a' - 'A';
		return str;
	}

	std::string toUpper(std::string str) {
		for (size_t i = 0, length = str.length(); i < length; ++i)
			if ('a' <= str[i] && str[i] <= 'z')
				str[i] -= 'a' - 'A';
		return str;
	}
}
