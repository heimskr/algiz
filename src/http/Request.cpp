#include <algorithm>

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
				else if (header_name == "Range")
					parseRange(header_content);
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

	void Request::parseRange(std::string_view content) {
		if (content.substr(0, 6) != "bytes=")
			throw ParseError("parseRange: invalid unit");
		content.remove_prefix(6);
		ranges.clear();
		try {
			while (!content.empty()) {
				const size_t separator = content.find(", ");
				std::string_view piece = content.substr(0, separator);

				const size_t hyphen = piece.find('-');

				size_t start = 0;
				size_t end = -1;

				if (hyphen == 0) {
					if (piece.size() == 1)
						throw ParseError("parseRange: invalid range");
					suffixLength = parseUlong(piece.substr(1));
				} else {
					start = parseUlong(piece.substr(0, hyphen));
					if (hyphen != piece.size() - 1)
						end = parseUlong(piece.substr(hyphen + 1));
					ranges.emplace_back(start, end);
				}

				content.remove_prefix(separator + 2);
			}
		} catch (const std::invalid_argument &) {
			throw ParseError("parseRange: number parsing failed");
		}
	}

	bool Request::valid(size_t total_size) {
		// Can't request a suffix larger than the total size.
		if (total_size < suffixLength)
			return false;

		if (ranges.empty())
			return true;

		// Sort all the ranges so we can go through them in linear time.
		std::sort(ranges.begin(), ranges.end(), [](const auto &left, const auto &right) {
			return std::get<0>(left) < std::get<1>(right);
		});

		// Once the ranges are sorted, no range can begin before the previous one ends.
		for (size_t i = 1, max = ranges.size(); i < max; ++i) {
			const size_t start = std::get<0>(ranges[i]);
			const size_t end = std::get<1>(ranges[i]);
			if (end <= start || start < std::get<1>(ranges[i - 1]) || total_size <= start || total_size < end)
				return false;
		}

		return std::get<1>(ranges.back()) <= total_size - suffixLength;
	}
}
