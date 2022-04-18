#include "error/ParseError.h"
#include "http/Client.h"
#include "Log.h"

namespace Algiz::HTTP {
	void Client::handleInput(const std::string &message) {
		if (mode != Mode::Content && message == "\r") {
			mode = Mode::Content;
			lineMode = false;
			if (headers.count("Content-Length") == 0) {
				handleRequest();
			}
		} else if (mode == Mode::Content) {
			content += message;
		} else {
			std::string trimmed = message;
			if (!trimmed.empty() && trimmed.back() == '\r')
				trimmed.pop_back();
			// SPAM(id << ": " << trimmed);
			if (mode == Mode::Headers) {
				const size_t separator = trimmed.find(": ");
				if (separator == std::string::npos)
					throw ParseError("Couldn't parser header line");
				const std::string header_name = trimmed.substr(0, separator);
				if (headers.count(header_name) != 0)
					headers.erase(header_name);
				const std::string header_data = trimmed.substr(separator + 2);
				INFO("\e[1m" << header_name << "\e[22m \e[2m->\e[22m [\e[1m" << header_data << "\e[22m]");
				headers.emplace(header_name, std::move(header_data));
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

	void Client::handleRequest() {
		if (method == "GET") {
			const std::string content("Hello there.");
			send("HTTP/1.1 200 OK\r\n");
			send("Content-Type: text/html; charset=UTF-8\r\n");
			send("Content-Length: " + std::to_string(content.size()) + "\r\n\r\n");
			send(content);
		} else {
			throw ParseError("Invalid method: " + method);
		}
	}

	std::unordered_set<std::string> Client::supportedMethods {"GET"};
}
