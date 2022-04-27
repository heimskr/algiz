#include <sys/socket.h>

#include "Core.h"
#include "Log.h"
#include "error/ParseError.h"
#include "http/Response.h"
#include "http/Server.h"
#include "net/SSLServer.h"
#include "util/Braille.h"
#include "util/Util.h"

namespace Algiz {
	static HTTP::Server * makeHTTP(std::unique_ptr<Server> &&server, const nlohmann::json &suboptions) {
		auto *http = new HTTP::Server(std::move(server), suboptions);
		INFO("Binding to " << suboptions.at("ip").get<std::string>() << " on port " << suboptions.at("port") << ".");

		if (suboptions.contains("plugins")) {
			for (const auto &[key, value]: suboptions.at("plugins").items()) {
				if (value.is_string()) {
					http->loadPlugin("plugin/" + value.get<std::string>() + SHARED_SUFFIX);
				} else {
					auto [path, plugin, object] =
						http->loadPlugin("plugin/" + value.at(0).get<std::string>() + SHARED_SUFFIX);
					
				}
			}
			http->preinitPlugins();
			http->postinitPlugins();
		}

		http->server->messageHandler = [server = http->server](int client, const std::string &message) {
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

	std::vector<ApplicationServer *> run(nlohmann::json &json) {
		std::vector<ApplicationServer *> out;
		out.reserve(4);

		std::cerr << braille;

		if (json.contains("http")) {
			const auto &suboptions = json.at("http");
			const std::string &ip = suboptions.at("ip");
			const uint16_t port = suboptions.at("port");
			const int af = ip.find(':') == std::string::npos? AF_INET : AF_INET6;
			out.emplace_back(makeHTTP(std::make_unique<Server>(af, ip, port, 1024), suboptions));
		}

		if (json.contains("https")) {
			const auto &suboptions = json.at("https");
			const std::string &ip = suboptions.at("ip");
			const uint16_t port = suboptions.at("port");
			const std::string &cert = suboptions.at("cert");
			const std::string &key = suboptions.at("key");
			const int af = ip.find(':') == std::string::npos? AF_INET : AF_INET6;
			out.emplace_back(makeHTTP(std::make_unique<SSLServer>(af, ip, port, cert, key, 1024), suboptions));
		}

		return out;
	}
}
