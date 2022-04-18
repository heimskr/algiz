#pragma once

#include <iostream>
#include <string>

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

		static std::string getTimestamp();
	};

	extern Logger log;
}

#define INFO(message) \
	do { ::Algiz::log << "\e[2m[\e[1m" << ::Algiz::Logger::getTimestamp() \
	                  << "\e[22;2m]\e[22m (\e[22;1;34mi\e[22;39m)\e[2m ::\e[22m "    \
	                  << message << std::endl; } while (false);

#define SPAM(message) \
	do { ::Algiz::log << "\e[2m[\e[1m" << ::Algiz::Logger::getTimestamp() \
	                  << "\e[22;2m]\e[22m (\e[22;1;35m_\e[22;39m)\e[2m :: "    \
	                  << message << "\e[22m" << std::endl; } while (false);
