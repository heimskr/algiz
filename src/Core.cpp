#include "error/ParseError.h"
#include "http/Response.h"
#include "http/Server.h"
#include "net/SSLServer.h"
#include "util/Braille.h"
#include "util/Util.h"
#include "Core.h"
#include "Log.h"

namespace Algiz {
	ApplicationServer * run(Options &options) {
		auto server = std::make_shared<SSLServer>(options.addressFamily, options.ip, options.port, "private.crt",
			"private.key", 1024);
		auto *http = new HTTP::Server(server, options);
		std::cerr << braille;
		INFO("Binding to " << options.ip << " on port " << options.port << ".");

		if (options.jsonObject.contains("plugins")) {
			for (const auto &[key, value]: options.jsonObject.at("plugins").items())
				http->loadPlugin("plugin/" + value.get<std::string>() + SHARED_SUFFIX);
			http->preinitPlugins();
			http->postinitPlugins();
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

		return http;
	}
}
