#pragma once

#include <iostream>

namespace Algiz {
	struct Logger {
		template <typename T>
		Logger & operator<<(const T &object) {
			std::cerr << object;
			return *this;
		}

		Logger & operator<<(std::ostream & (*fn)(std::ostream &)) {
			fn(std::cerr);
			return *this;
		}

		Logger & operator<<(std::ostream & (*fn)(std::ios &)) {
			fn(std::cerr);
			return *this;
		}

		Logger & operator<<(std::ostream & (*fn)(std::ios_base &)) {
			fn(std::cerr);
			return *this;
		}
	};

	extern Logger log;
}
