#pragma once

#include <map>
#include <string>

namespace Algiz::HTTP {
	class Request {
		private:
			enum class Mode {Method, Headers, Content};
			Mode mode = Mode::Method;

			size_t contentLength = 0;
			size_t lengthRemaining = 0;

		public:
			enum class Method {GET, HEAD, PUT, POST};
			enum class HandleResult {Continue, DisableLineMode, Done};

			Method method;
			std::string path;
			std::string version;
			std::string content;
			std::string charset;
			std::map<std::string, std::string> headers;

			Request() = default;

			HandleResult handleLine(std::string_view);
			bool isComplete() const;
	};
}
