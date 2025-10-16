#pragma once

#include <map>
#include <string>
#include <variant>

namespace Algiz::HTTP {
	class Response {
		private:
			static std::map<int, std::string> codeDescriptions;

			static std::string generate500();

		public:
			int code;
			std::string mime = "text/html";
			std::variant<std::string, std::string_view> content;
			std::string charset;
			std::map<std::string, std::string> headers;
			bool noContentType = false;

			Response(int code, std::string content, std::string_view mime = "text/html");
			Response(int code, std::string_view content, std::string_view mime = "text/html");
			Response(int code, const char *content, std::string_view mime = "text/html");

			Response & setMIME(std::string);
			Response & setCharset(std::string);
			Response & setHeaders(decltype(headers));
			Response & setHeader(const std::string &header, std::string value);
			Response & setClose(bool = true);
			Response & setNoContentType(bool = true);
			Response & setAcceptRanges(bool = true);
			Response & setLastModified(time_t);

			std::string_view contentView() const;

			std::string & operator[](const std::string &);
			const std::string & operator[](const std::string &) const;

			operator std::string() const;
			[[nodiscard]] std::string noContent() const;
	};
}
