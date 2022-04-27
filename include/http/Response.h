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
			std::variant<std::string, std::string_view> content {};
			std::string charset {};
			std::map<std::string, std::string> headers {};
			bool noContentType = false;

			Response(int code_, const std::string &content_);
			Response(int code_, std::string_view content_);
			Response(int code_, const char *content_);

			Response & setMIME(const std::string &);
			Response & setCharset(const std::string &);
			Response & setHeaders(const decltype(headers) &);
			Response & setHeader(const std::string &, const std::string &);
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
