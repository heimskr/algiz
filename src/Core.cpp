#include "error/ParseError.h"
#include "http/Server.h"
#include "Core.h"
#include "Log.h"

namespace Algiz {
	Server * run(const Options &options) {
		auto *server = new HTTP::Server(options);
		server->messageHandler = [server](int client, const std::string &message) {
			try {
				server->getClients().at(client)->handleInput(message);
			} catch (const ParseError &error) {
				INFO("Disconnecting client " << client << ": " << error.what());
				server->send(client, "HTTP/1.1 400 Bad Request");
				server->removeClient(client);
			}
		};

		return server;
	}
}
