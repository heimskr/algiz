#include <charconv>
#include <cstring>

#include "util/Util.h"
#include "Log.h"

namespace Algiz {
	std::vector<std::string_view> split(const std::string_view &str, const std::string &delimiter, bool condense) {
		if (str.empty())
			return {};

		size_t next = str.find(delimiter);
		if (next == std::string::npos)
			return {str};

		std::vector<std::string_view> out;
		const size_t delimiter_length = delimiter.size();
		size_t last = 0;

		out.push_back(str.substr(0, next));

		while (next != std::string::npos) {
			last = next;
			next = str.find(delimiter, last + delimiter_length);
			std::string_view sub = str.substr(last + delimiter_length, next - last - delimiter_length);
			if (!sub.empty() || !condense)
				out.push_back(sub);
		}

		return out;
	}

	long parseLong(const std::string &str, int base) {
		const char *c_str = str.c_str();
		char *end = nullptr;
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

	long parseLong(std::string_view view, int base) {
		long out;
		auto result = std::from_chars(view.begin(), view.end(), out, base);
		if (result.ec == std::errc::invalid_argument)
			throw std::invalid_argument("Not an integer: \"" + std::string(view) + "\"");
		return out;
	}

	unsigned long parseUlong(const std::string &str, int base) {
		const char *c_str = str.c_str();
		char *end = nullptr;
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

	unsigned long parseUlong(std::string_view view, int base) {
		unsigned long out;
		auto result = std::from_chars(view.begin(), view.end(), out, base);
		if (result.ec == std::errc::invalid_argument)
			throw std::invalid_argument("Not an integer: \"" + std::string(view) + "\"");
		return out;
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

	std::string unescape(const std::string_view &path, bool plus_to_space) {
		if (path.find('%') == std::string::npos && !(plus_to_space && path.find('+') != std::string::npos))
			return std::string(path);

		std::string out;

		for (size_t i = 0, size = path.size(); i < size; ++i) {
			const char ch = path[i];
			if (plus_to_space && ch == '+') {
				out += ' ';
				continue;
			}

			if (ch != '%' || size - 3 < i) {
				out += ch;
				continue;
			}

			const char next  = path[i + 1];
			const char after = path[i + 2];
			if (!std::isxdigit(next) || !std::isxdigit(after)) {
				WARN("Not hex: " << next << " or " << after);
				out += ch;
				continue;
			}

			char to_add = 0;

			if ('a' <= next && next <= 'f')
				to_add += (next - 'a' + 10) << 4;
			else if ('A' <= next && next <= 'F')
				to_add += (next - 'A' + 10) << 4;
			else
				to_add += (next - '0') << 4;

			if ('a' <= after && after <= 'f')
				to_add += after - 'a' + 10;
			else if ('A' <= after && after <= 'F')
				to_add += after - 'A' + 10;
			else
				to_add += after - '0';

			INFO("Adding '" << to_add << "'");
			out += to_add;
			i += 2;
		}

		INFO("Returning \"" << out << "\"");
		return out;
	}
}
