#include "http/Response.h"

namespace Algiz::HTTP {
	std::string Response::generate500() {
		const std::string body("Internal Server Error");
		return std::string("HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html; charset=UTF-8\r\n")
		     + "Content-Length: " + std::to_string(body.size()) + "\r\n"
		     + "Connection: close\r\n"
		     + "\r\n" + body;
	}

	Response::Response(int code_, const std::string &content_): code(code_), content(content_) {
		setClose(true);
	}

	Response & Response::setMIME(const std::string &mime_) {
		mime = mime_;
		return *this;
	}

	Response & Response::setCharset(const std::string &charset_) {
		charset = charset_;
		return *this;
	}

	Response & Response::setHeaders(const decltype(headers) &headers_) {
		headers = headers_;
		return *this;
	}

	Response & Response::setHeader(const std::string &header, const std::string &value) {
		headers[header] = value;
		return *this;
	}

	Response & Response::setClose(bool close) {
		if (close)
			return setHeader("Connection", "close");
		if (headers.count("Connection") != 0 && headers.at("Connection") == "close")
			headers.erase("Connection");
		return *this;
	}

	Response & Response::setNoContentType(bool value) {
		noContentType = value;
		return *this;
	}

	std::string & Response::operator[](const std::string &header_name) {
		return headers[header_name];
	}

	const std::string & Response::operator[](const std::string &header_name) const {
		return headers.at(header_name);
	}

	Response::operator std::string() const {
		if (codeDescriptions.count(code) == 0)
			return generate500();
		
		std::string out;
		out.reserve(content.size() + 1024);
		out = "HTTP/1.1 " + std::to_string(code) + " " + codeDescriptions.at(code) + "\r\n";
		if (!noContentType && headers.count("Content-Type") == 0)
		    out += "Content-Type: " + mime + (charset.empty()? "" : "; charset = " + charset) + "\r\n";
		if (headers.count("Content-Length") == 0)
			out += "Content-Length: " + std::to_string(content.size()) + "\r\n";
		for (const auto &[header, value]: headers)
			out += header + ": " + value + "\r\n";
		out += "\r\n" + content;
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
