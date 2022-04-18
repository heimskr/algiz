#pragma once

#include <stdexcept>

namespace Algiz {
	class ResolutionError: public std::runtime_error {
		public:
			int statusCode;

			ResolutionError(int status_code):
				std::runtime_error("Resolution failed"), statusCode(status_code) {}

			const char * what() const throw() override {
				return gai_strerror(statusCode);
			}
	};
}
