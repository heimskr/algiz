#include <algorithm>

#include "error/ParseError.h"
#include "error/UnsupportedMethod.h"
#include "http/Client.h"
#include "http/Request.h"
#include "http/Server.h"
#include "util/Base64.h"
#include "util/Util.h"

#include "Log.h"
#include "Options.h"

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
			if (headers.contains("content-length")) {
				client.maxRead = contentLength;
				return HandleResult::DisableLineMode;
			}
			return HandleResult::Done;
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
				auto full_path = line.substr(0, next_space);
				path = getPath(full_path);
				parameters = getParameters(full_path);
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
				std::string header_name_string = toLower(header_name);

				if (headers.contains(header_name_string)) {
					if (headers[header_name_string].empty()) {
						headers[header_name_string] = header_content;
					} else {
						headers[header_name_string] += " ";
						headers[header_name_string] += header_content;
					}
				} else {
					headers.emplace(header_name_string, header_content);
				}

				if (header_name == "Content-Length") {
					try {
						lengthRemaining = contentLength = parseUlong(header_content);
						if (method == Method::POST) {
							const auto post_max_opt = client.server.getOption<size_t>(path, "postMax");
							const size_t post_max = post_max_opt? *post_max_opt : POST_MAX;
							if (post_max < lengthRemaining)
								throw ParseError("POST length too long: " + std::to_string(lengthRemaining));
						}
					} catch (const std::invalid_argument &err) {
						throw ParseError(err.what());
					}
				} else if (header_name == "Range") {
					parseRange(header_content);
				} else if (header_name == "Connection") {
					client.keepAlive = header_content != "close";
				}
				break;
			}

			case Mode::Content: {
				if (lengthRemaining < line.size()) {
					content += line.substr(lengthRemaining);
					lengthRemaining = 0;
				} else if (lengthRemaining == line.size()) {
					content += line;
					lengthRemaining = 0;
				}

				if (lengthRemaining == 0) {
					if (method == Method::POST)
						absorbPOST();
					mode = Mode::Method;
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

				if (separator == std::string_view::npos)
					break;

				content.remove_prefix(separator + 2);
			}
		} catch (const std::invalid_argument &) {
			throw ParseError("parseRange: number parsing failed");
		}
	}

	void Request::absorbPOST() {
		if (auto iter = headers.find("content-type"); iter != headers.end()) {
			if (iter->second != "application/x-www-form-urlencoded") {
				return;
			}
		}

		postParameters = getParameters(content, false);
		content.clear();
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
			const size_t end   = std::get<1>(ranges[i]);
			const size_t prev_end = std::get<1>(ranges[i - 1]);
			if (end <= start || start < prev_end || total_size <= start || total_size < end)
				return false;
		}

		return std::get<1>(ranges.back()) <= total_size - suffixLength;
	}

	std::string_view Request::getPath(std::string_view path) {
		return path.substr(0, path.find('?'));
	}

	std::map<std::string, std::string> Request::getParameters(std::string_view path, bool chop_question_mark) {
		std::map<std::string, std::string> parameters;

		if (chop_question_mark) {
			const size_t question_mark = path.find('?');
			if (question_mark == std::string_view::npos)
				return parameters;

			path.remove_prefix(question_mark + 1);
		}

		size_t ampersand = path.find('&');

		auto add_parameter = [&] {
			std::string_view pair = path.substr(0, ampersand);
			if (pair.empty())
				return;
			const size_t equals = pair.find('=');
			if (equals == std::string_view::npos)
				parameters[unescape(pair)] = "";
			else
				parameters[unescape(pair.substr(0, equals))] = unescape(pair.substr(equals + 1));
		};

		while (ampersand != std::string_view::npos) {
			add_parameter();
			path.remove_prefix(ampersand + 1);
			ampersand = path.find('&');
		}

		add_parameter();
		return parameters;
	}

	AuthenticationResult Request::checkAuthentication(std::string_view username, std::string_view password) const {
		auto iter = headers.find("authorization");
		if (iter == headers.end()) {
			return AuthenticationResult::Missing;
		}
		std::string_view auth = iter->second;
		if (auth.substr(0, 6) != "Basic ") {
			return AuthenticationResult::Malformed;
		}
		auth.remove_prefix(6);
		const std::string decoded_str = base64Decode(auth);
		const std::string_view decoded = decoded_str;
		const size_t colon = decoded.find(':');
		if (colon == std::string::npos) {
			return AuthenticationResult::Malformed;
		}
		if (decoded.substr(0, colon) != username) {
			return AuthenticationResult::BadUsername;
		}
		if (decoded.substr(colon + 1) != password) {
			return AuthenticationResult::BadPassword;
		}
		return AuthenticationResult::Success;
	}

	std::string_view Request::getHeader(const std::string &name) const {
		if (auto iter = headers.find(name); iter != headers.end()) {
			return iter->second;
		}

		return {};
	}
}
