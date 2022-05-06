#pragma once

#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace Algiz::HTTP {
	enum class AuthenticationResult {Invalid, Missing, Malformed, BadUsername, BadPassword, Success};

	class Request {
		private:
			enum class Mode {Method, Headers, Content};
			Mode mode = Mode::Method;

			size_t contentLength = 0;
			size_t lengthRemaining = 0;

			void parseRange(std::string_view);

			/** When given "/foo?bar=baz&quux=hello", returns "/foo" */
			static std::string_view getPath(std::string_view);

			/** Takes in a string like "/foo?bar=baz&quux=hello" */
			static std::map<std::string, std::string> getParameters(std::string_view);

		public:
			enum class Method {Invalid, GET, HEAD, PUT, POST};
			enum class HandleResult {Continue, DisableLineMode, Done};

			Method method = Method::Invalid;
			std::string path;
			std::string version;
			std::string content;
			std::string charset;
			std::map<std::string, std::string> headers;
			std::map<std::string, std::string> parameters;
			std::vector<std::tuple<size_t, size_t>> ranges;
			size_t suffixLength = 0;

			Request() = default;

			HandleResult handleLine(std::string_view);
			bool valid(size_t total_size);
			AuthenticationResult checkAuthentication(std::string_view username, std::string_view password) const;
	};
}
