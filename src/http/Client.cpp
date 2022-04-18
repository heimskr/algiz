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
			std::cerr << id << "[" << trimmed << "]\n";
			if (mode == Mode::Headers) {

			} else if (mode == Mode::Method) {
				const size_t space = trimmed.find(' ');
				if (space == std::string::npos)
					throw ParseError("Bad method line");
				std::string method(trimmed.c_str(), space);
				
				throw ParseError("Invalid method: " + method);
			} else
				throw std::runtime_error("Invalid HTTP client mode: " + std::to_string(int(mode)));
		}
	}
}
