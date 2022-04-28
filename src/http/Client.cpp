#include "Log.h"
#include "error/ParseError.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	void Client::send(const std::string &message) {
		server.server->send(id, message);
	}

	void Client::handleInput(const std::string &message) {
		if (isWebSocket) {
			std::cerr << std::string(16, '=') << ' ' << message.size() << ' ' << std::string(16, '=') << '\n';
			for (char ch: message)
				std::cerr << hex(ch) << ' ' << escapeANSI(std::string(1, ch)) << '\n';
			std::cerr << std::string(34 + std::to_string(message.size()).size(), '=') << '\n';
		} else {
			const auto result = request.handleLine(message);
			if (result == Request::HandleResult::DisableLineMode)
				lineMode = false;
			else if (result == Request::HandleResult::Done)
				handleRequest();
		}
	}

	void Client::handleRequest() {
		switch (request.method) {
			case Request::Method::GET:
				server.handleGet(*this, request);
				break;
			default:
				throw ParseError("Invalid method: " + std::to_string(int(request.method)));
		}
	}

	void Client::onMaxLineSizeExceeded() {
		send(Response(413, "Payload too large"));
	}

	std::unordered_set<std::string> Client::supportedMethods {"GET"};
}
