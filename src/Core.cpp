#include "error/ParseError.h"
#include "http/Response.h"
#include "http/Server.h"
#include "Core.h"
#include "Log.h"

namespace Algiz {
	Server * run(const Options &options) {
		auto *server = new HTTP::Server(options);
		INFO("Binding to " << options.ip << " on port " << options.port << ".");

		server->loadPlugins("./plugin");
		server->preinitPlugins();
		server->postinitPlugins();

		server->messageHandler = [server](int client, const std::string &message) {
			try {
				server->getClients().at(client)->handleInput(message);
			} catch (const ParseError &error) {
				INFO("Disconnecting client " << client << ": " << error.what());
				server->send(client, HTTP::Response(400, "Couldn't parse request."));
				server->removeClient(client);
			}
		};

		return server;
	}
}
