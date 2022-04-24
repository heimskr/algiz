#pragma once

#include <map>
#include <string>

namespace Algiz::HTTP {
	class Request {
		public:
			enum class Method {GET, HEAD, PUT, POST};
			Method method;
			const std::string full;
			std::string_view path;
			std::string_view version;
			std::string_view content;
			std::string_view charset;
			std::map<std::string, std::string> headers;

			/** Constructs a Request from a string representing the full request. */
			explicit Request(const std::string &full_);
			explicit Request(std::string &&full_);
	};
}
