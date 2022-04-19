#include "error/ParseError.h"
#include "http/Response.h"
#include "http/Server.h"
#include "util/Util.h"
#include "Core.h"
#include "Log.h"

namespace Algiz {
	Server * run(const Options &options) {
		auto *server = new HTTP::Server(options);
		INFO("Binding to " << options.ip << " on port " << options.port << ".");

		if (options.jsonObject.contains("plugins")) {
			for (const auto &[key, value]: options.jsonObject.at("plugins").items())
				server->loadPlugin("plugin/" + value.get<std::string>() + SHARED_SUFFIX);
			server->preinitPlugins();
			server->postinitPlugins();
		}

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
