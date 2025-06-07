#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace Algiz {
	/** Splits a string by a given delimiter. If condense is true, empty strings won't be included in the output. */
	std::vector<std::string_view> split(const std::string_view &str, const std::string &delimiter,
		bool condense = true);

	long parseLong(const std::string &, int base = 10);
	long parseLong(const char *, int base = 10);
	long parseLong(std::string_view, int base = 10);
	unsigned long parseUlong(const std::string &, int base = 10);
	unsigned long parseUlong(const char *, int base = 10);
	unsigned long parseUlong(std::string_view, int base = 10);
	std::string toLower(std::string_view);
	std::string toUpper(std::string_view);
	std::string unescape(std::string_view, bool plus_to_space = true);
	bool isNumeric(char);
	bool isNumeric(std::string_view);

	/** Replaces '\x1b' bytes with "\x1b[2m\\x1b\x1b[22m". Also replaces some special whitespace characters with their
	 *  escaped representations. */
	std::string escapeANSI(std::string_view);
	std::string escapeHTML(std::string_view);
	std::string escapeURL(std::string_view);
	std::string charHex(uint8_t);
	std::string escapeQuotes(std::string_view);

	template <size_t BL = 128>
	std::string formatTime(const char *format,
	                       std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())) {
		tm now_tm;
		localtime_r(&now, &now_tm);
		char buffer[BL];
		strftime(buffer, sizeof(buffer), format, &now_tm);
		return buffer;
	}

	template <template <typename...> typename C, typename T, typename... CA, typename Fn,
	          template <typename...> typename V = std::vector>
	auto map(const C<T, CA...> &input, Fn function) -> V<decltype(function(input.front()))> {
		V<decltype(function(input.front()))> out;
		std::transform(input.begin(), input.end(), std::back_inserter(out), function);
		return out;
	}

	template<typename R, typename O1, typename O2, typename... A>
	auto bind(O1 &obj, R(O2::*func)(A...)) {
		return std::function<R(A...)>([&obj, func](A && ...args) -> R {
			return std::invoke(func, obj, std::forward<A>(args)...);
		});
	}

	template <typename T>
	std::string hex(const T &n) {
		std::stringstream ss;
		ss << std::hex << n;
		std::string out = std::move(ss).str();
		if (out.size() == 1) {
			return "0" + out;
		}
		return out;
	}

	std::string hexString(std::string_view);

	template <template <typename...> typename C, typename T, typename D>
	std::string join(const C<T> &container, D &&delimiter) {
		std::stringstream ss;
		bool first = true;
		for (const T &item: container) {
			if (first)
				first = false;
			else
				ss << delimiter;
			ss << item;
		}
		return ss.str();
	}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	#define SHARED_SUFFIX ".dll"
#elif defined(__APPLE__)
	#define SHARED_SUFFIX ".dylib"
#else
	#define SHARED_SUFFIX ".so"
#endif
}
