#pragma once

#include <map>
#include <string>

namespace Algiz::HTTP {
	class Response {
		private:
			static std::map<int, std::string> codeDescriptions;

			static std::string generate500();

		public:
			int code;
			std::string mime = "text/html";
			std::string content;
			std::string charset;
			std::map<std::string, std::string> headers;

			Response(int code_, const std::string &content_);

			Response & setMIME(const std::string &);
			Response & setCharset(const std::string &);
			Response & setHeaders(const decltype(headers) &);
			Response & setHeader(const std::string &, const std::string &);
			Response & setClose(bool = true);

			operator std::string() const;
	};
}
