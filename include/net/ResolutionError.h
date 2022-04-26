#pragma once

#include <stdexcept>

namespace Algiz {
	class ResolutionError: public std::runtime_error {
		public:
			int statusCode;

			explicit ResolutionError(int status_code):
				std::runtime_error("Resolution failed"), statusCode(status_code) {}

			[[nodiscard]] const char * what() const noexcept override {
				return gai_strerror(statusCode);
			}
	};
}
