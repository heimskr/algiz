#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace Algiz {
	struct StringVector: public std::vector<std::string> {
		using std::vector<std::string>::vector;

		StringVector(const std::vector<std::string> &strings): std::vector<std::string>(strings) {}
		StringVector(std::vector<std::string> &&strings): std::vector<std::string>(strings) {}
		StringVector(std::vector<std::string_view> &&strings);

		std::string join(const char *delimiter) const;

		bool operator==(std::initializer_list<const char *>) const;

		template <typename T>
		bool contains(const T &needle) const {
			for (const auto &string: *this) {
				if (string == needle) {
					return true;
				}
			}
			return false;
		}
	};
}
