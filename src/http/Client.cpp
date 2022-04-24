#include "error/ParseError.h"
#include "http/Client.h"
#include "http/Server.h"
#include "Log.h"

namespace Algiz::HTTP {
	void Client::send(const std::string &message) {
		server.server->send(id, message);
	}

	void Client::handleInput(const std::string &message) {
		request.emplace(message);
	}

	void Client::handleRequest() {
		if (!request)
			throw std::runtime_error("No request to handle.");

		switch (request->method) {
			case Request::Method::GET:
				server.handleGet(*this, *request);
				break;
			default:
				throw ParseError("Invalid method: " + std::to_string(int(request->method)));
		}
	}

	std::unordered_set<std::string> Client::supportedMethods {"GET"};
}
