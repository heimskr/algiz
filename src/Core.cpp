#include "error/ParseError.h"
#include "net/Server.h"
#include "Core.h"
#include "Log.h"

namespace Algiz {
	Server * run(const Options &options) {
		auto *server = new Server(options.addressFamily, options.ip, options.port, 1024);
		server->messageHandler = [server](int client, const std::string &message) {
			try {
				server->getClients().at(client)->handleInput(message);
			} catch (const ParseError &error) {
				log << "Disconnecting client " << client << ": " << error.what() << "\n";
				server->removeClient(client);
			}
		};

		return server;
	}
}
