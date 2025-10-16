#include "http/Response.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	std::string Response::generate500() {
		const static std::string_view body = "Internal Server Error";
		return std::format(
			"HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html; charset=UTF-8\r\n"
			"Content-Length: {}\r\n"
			"Connection: close\r\n"
			"\r\n{}", body.size(), body);
	}

	Response::Response(int code, std::string content, std::string_view mime):
		code(code),
		mime(mime),
		content(std::move(content)) {
			setClose(true);
		}

	Response::Response(int code, std::string_view content, std::string_view mime):
		code(code),
		mime(mime),
		content(content) {
			setClose(true);
		}

	Response::Response(int code, const char *content, std::string_view mime):
		code(code),
		mime(mime),
		content(std::string(content)) {
			setClose(true);
		}

	Response & Response::setMIME(std::string mime_) {
		mime = std::move(mime_);
		return *this;
	}

	Response & Response::setCharset(std::string charset_) {
		charset = std::move(charset_);
		return *this;
	}

	Response & Response::setHeaders(decltype(headers) headers_) {
		headers = std::move(headers_);
		return *this;
	}

	Response & Response::setHeader(const std::string &header, std::string value) {
		headers[toLower(header)] = std::move(value);
		return *this;
	}

	Response & Response::setClose(bool close) {
		if (close) {
			return setHeader("Connection", "close");
		}

		if (auto iter = headers.find("connection"); iter != headers.end() && iter->second == "close") {
			headers.erase(iter);
		}

		return *this;
	}

	Response & Response::setNoContentType(bool value) {
		noContentType = value;
		return *this;
	}

	Response & Response::setAcceptRanges(bool value) {
		if (value) {
			headers["accept-ranges"] = "bytes";
		} else {
			headers.erase("accept-ranges");
		}
		return *this;
	}

	Response & Response::setLastModified(time_t when) {
		headers["last-modified"] = formatTime("%a, %-d %b %Y %T %Z", when);
		return *this;
	}

	std::string_view Response::contentView() const {
		if (std::holds_alternative<std::string>(content)) {
			return std::string_view(std::get<std::string>(content));
		}
		return std::get<std::string_view>(content);
	}

	std::string & Response::operator[](const std::string &header_name) {
		return headers[header_name];
	}

	const std::string & Response::operator[](const std::string &header_name) const {
		return headers.at(header_name);
	}

	Response::operator std::string() const {
		if (std::holds_alternative<std::string>(content)) {
			return noContent() + std::get<std::string>(content);
		}

		std::string no_content = noContent();
		no_content += std::get<std::string_view>(content);
		return no_content;
	}

	std::string Response::noContent() const {
		if (!codeDescriptions.contains(code)) {
			return generate500();
		}

		std::string out;
		const size_t content_size = std::holds_alternative<std::string>(content)? std::get<std::string>(content).size() : std::get<std::string_view>(content).size();
		out.reserve(content_size + 1024);
		out = std::format("HTTP/1.1 {} {}\r\n", code, codeDescriptions.at(code));

		if (!noContentType && !headers.contains("content-type")) {
			out += std::format("Content-Type: {}{}\r\n", mime, charset.empty()? "" : "; charset=" + charset);
		}

		if (!headers.contains("content-length")) {
			out += std::format("Content-Length: {}\r\n", content_size);
		}

		for (const auto &[header, value]: headers) {
			out += std::format("{}: {}\r\n", header, value);
		}

		out += "\r\n";
		return out;
	}

	std::map<int, std::string> Response::codeDescriptions {
		{100, "Continue"},
		{101, "Switching Protocols"},
		{102, "Processing"},
		{103, "Early Hints"},
		{200, "OK"},
		{201, "Created"},
		{202, "Accepted"},
		{203, "Non-Authoritative Information"},
		{204, "No Content"},
		{205, "Reset Content"},
		{206, "Partial Content"},
		{207, "Multi-Status"},
		{208, "Already Reported"},
		{226, "IM Used"},
		{300, "Multiple Choices"},
		{301, "Moved Permanently"},
		{302, "Found"},
		{303, "See Other"},
		{304, "Not Modified"},
		{305, "Use Proxy"},
		{306, "Switch Proxy"},
		{307, "Temporary Redirect"},
		{308, "Permanent Redirect"},
		{400, "Bad Request"},
		{401, "Unauthorized"},
		{402, "Payment Required"},
		{403, "Forbidden"},
		{404, "Not Found"},
		{405, "Method Not Allowed"},
		{406, "Not Acceptable"},
		{407, "Proxy Authentication Required"},
		{408, "Request Timeout"},
		{409, "Conflict"},
		{410, "Gone"},
		{411, "Length Required"},
		{412, "Precondition Failed"},
		{413, "Payload Too Large"},
		{414, "URI Too Long"},
		{415, "Unsupported Media Type"},
		{416, "Range Not Satisfiable"},
		{417, "Expectation Failed"},
		{418, "I'm a teapot"},
		{421, "Misdirected Request"},
		{422, "Unprocessable Entity"},
		{423, "Locked"},
		{424, "Failed Dependency"},
		{425, "Too Early"},
		{426, "Upgrade Required"},
		{428, "Precondition Required"},
		{429, "Too Many Requests"},
		{431, "Request Header Fields Too Large"},
		{451, "Unavailable For Legal Reasons"},
		{500, "Internal Server Error"},
		{501, "Not Implemented"},
		{502, "Bad Gateway"},
		{503, "Service Unavailable"},
		{504, "Gateway Timeout"},
		{505, "HTTP Version Not Supported"},
		{506, "Variant Also Negotiates"},
		{507, "Insufficient Storage"},
		{508, "Loop Detected"},
		{510, "Not Extended"},
		{511, "Network Authentication Required"},
	};
}
