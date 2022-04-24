#include "error/ParseError.h"
#include "error/UnsupportedMethod.h"
#include "http/Request.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz::HTTP {
	Request::HandleResult Request::handleLine(std::string_view line) {
		if (mode != Mode::Content) {
			if (!line.empty() && line.back() == '\n')
				line.remove_suffix(1);
			if (!line.empty() && line.back() == '\r')
				line.remove_suffix(1);
		}

		if (line.empty() && mode == Mode::Headers) {
			mode = Mode::Content;
			return headers.count("Content-Length") == 0? HandleResult::Done : HandleResult::DisableLineMode;
		}

		switch (mode) {
			case Mode::Method: {
				const size_t first_space = line.find(' ');

				if (first_space == std::string_view::npos)
					throw ParseError("Bad method line: can't determine method");

				const std::string_view method_view = line.substr(0, first_space);

				if (method_view == "GET")
					method = Method::GET;
				else if (method_view == "HEAD")
					method = Method::HEAD;
				else if (method_view == "PUT")
					method = Method::PUT;
				else if (method_view == "POST")
					method = Method::POST;
				else
					throw UnsupportedMethod(std::string(method_view));

				line = line.substr(first_space + 1);
				const size_t next_space = line.find(' ');
				if (next_space == std::string_view::npos)
					throw ParseError("Bad method line: can't determine path");
				path = line.substr(0, next_space);
				version = line.substr(next_space + 1);
				if (version != "HTTP/1.1" && version != "HTTP/1.0")
					throw ParseError("Invalid HTTP version: " + version);
				mode = Mode::Headers;
				break;
			}

			case Mode::Headers: {
				const size_t separator = line.find(": ");
				if (separator == std::string_view::npos)
					throw ParseError("Invalid HTTP header: no separator");
				std::string_view header_name = line.substr(0, separator);
				std::string_view header_content = line.substr(separator + 2);
				headers[std::string(header_name)] = header_content;
				if (header_name == "Content-Length")
					try {
						lengthRemaining = contentLength = parseUlong(header_content.begin());
					} catch (const std::invalid_argument &err) {
						throw ParseError(err.what());
					}
				break;
			}

			case Mode::Content: {
				if (lengthRemaining < line.size()) {
					content += line.substr(lengthRemaining);
					lengthRemaining = 0;
					return HandleResult::Done;
				}

				if (lengthRemaining == line.size()) {
					content += line;
					lengthRemaining = 0;
					return HandleResult::Done;
				}

				content += line;
				lengthRemaining -= line.size();
				break;
			}
		}

		return HandleResult::Continue;
	}
}
