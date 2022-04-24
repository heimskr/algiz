#include "error/ParseError.h"
#include "error/UnsupportedMethod.h"
#include "http/Request.h"
#include "Log.h"

namespace Algiz::HTTP {
	Request::Request(const std::string &full_): Request(std::string(full_)) {}

	Request::Request(std::string &&full_): full(std::move(full_)) {
		enum class Mode {Method, Headers, Content};

		Mode mode = Mode::Method;
		
		std::string_view view(full);

		SPAM("[[" << view << "]]");

		try {
			for (;;) {
				if (view.empty())
					break;

				switch (mode) {
					case Mode::Method: {
						const size_t first_space = view.find(' ');

						if (first_space == std::string_view::npos)
							throw ParseError("Bad method line: can't determine method");

						const std::string_view method_view = view.substr(0, first_space);

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

						view = view.substr(first_space + 1);
						const size_t next_space = view.find(' ');

						if (next_space == std::string_view::npos)
							throw ParseError("Bad method line: can't determine path");
						
						path = view.substr(0, next_space);

						const size_t carriage_return = view.find('\r');
						if (carriage_return == std::string_view::npos)
							throw ParseError("Invalid HTTP request: method line ended too early");

						version = view.substr(next_space + 1, carriage_return - next_space - 1);

						if (version != "HTTP/1.1" && version != "HTTP/1.0")
							throw ParseError("Invalid HTTP version: " + std::string(version));

						const size_t newline = view.find('\n');
						if (newline == std::string_view::npos || newline != carriage_return + 1) {
							WARN(carriage_return << ", " << newline);
							throw ParseError("Invalid HTTP request: invalid line terminator");
						}

						view = view.substr(newline + 1);
						mode = Mode::Headers;
						break;
					}

					case Mode::Headers: {
						const size_t separator = view.find(": ");
						if (separator == std::string_view::npos)
							throw ParseError("Invalid HTTP header: no separator");

						std::string_view header_name = view.substr(0, separator);
						
						view = view.substr(separator + 2);
						const size_t newline = view.find('\n');
						if (newline == std::string_view::npos)
							throw ParseError("Invalid HTTP header: no newline");

						// There needs to be room for a carriage return.
						if (newline == 0)
							throw ParseError("Invalid HTTP header: newline too early");

						headers[std::string(header_name)] = view.substr(0, newline - 1);
						view = view.substr(newline + 1);
						break;
					}

					case Mode::Content: {
						content = view;
						view = {};
						break;
					};
				}

				if ((mode == Mode::Method || mode == Mode::Headers) && !view.empty() && view.front() == '\r') {
					mode = Mode::Content;
					view = view.substr(2);
				}
			}
		} catch (const std::out_of_range &) {
			throw ParseError("Invalid HTTP request: out of range");
		}
	}
}
