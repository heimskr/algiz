#include "error/ParseError.h"
#include "http/Client.h"
#include "Log.h"

namespace Algiz::HTTP {
	void Client::handleInput(const std::string &message) {
		if (message == "\r") {
			mode = Mode::Content;
			lineMode = false;
		} else if (mode == Mode::Content) {
			content += message;
		} else {
			std::string trimmed = message;
			if (!trimmed.empty() && trimmed.back() == '\r')
				trimmed.pop_back();
			SPAM(id << ": " << trimmed);
			if (mode == Mode::Headers) {

			} else if (mode == Mode::Method) {
				const size_t space = trimmed.find(' ');
				if (space == std::string::npos)
					throw ParseError("Bad method line");
				method = {trimmed.c_str(), space};
				if (supportedMethods.contains(method)) {
					INFO("Setting method to " << method);
					const size_t second_space = trimmed.find(' ', space + 1);
					if (space != std::string::npos) {
						path = {trimmed.c_str(), space + 1, second_space - space - 1};
						if (!path.empty() && path.front() == '/') {
							const std::string version = trimmed.substr(second_space + 1);
							if (version != "HTTP/1.1" && version != "HTTP/1.0")
								throw ParseError("Invalid HTTP version");
							mode = Mode::Headers;
							return;
						}
					}
					throw ParseError("Bad GET path");
				}
				throw ParseError("Invalid method: " + method);
			} else
				throw std::runtime_error("Invalid HTTP client mode: " + std::to_string(int(mode)));
		}
	}

	std::unordered_set<std::string> Client::supportedMethods {"GET"};
}
