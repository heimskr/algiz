#include <sys/socket.h>

#include "error/ParseError.h"
#include "http/Response.h"
#include "http/Server.h"
#include "net/SSLServer.h"
#include "util/Braille.h"
#include "util/Util.h"
#include "Core.h"
#include "Log.h"

namespace Algiz {
	static HTTP::Server * makeHTTP(const std::shared_ptr<Server> &server, const nlohmann::json &suboptions) {
		auto *http = new HTTP::Server(server, suboptions);
		INFO("Binding to " << suboptions.at("ip") << " on port " << suboptions.at("port") << ".");

		if (suboptions.contains("plugins")) {
			for (const auto &[key, value]: suboptions.at("plugins").items())
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

	std::vector<ApplicationServer *> run(nlohmann::json &json) {
		std::vector<ApplicationServer *> out;
		out.reserve(4);

		std::cerr << braille;

		if (json.contains("http")) {
			const auto &suboptions = json.at("http");
			const std::string &ip = suboptions.at("ip");
			const uint16_t port = suboptions.at("port");
			const int af = ip.find(':') == std::string::npos? AF_INET : AF_INET6;
			auto server = std::make_shared<Server>(af, ip, port, 1024);
			out.emplace_back(makeHTTP(server, suboptions));
		}

		if (json.contains("https")) {
			const auto &suboptions = json.at("https");
			const std::string &ip = suboptions.at("ip");
			const uint16_t port = suboptions.at("port");
			const std::string &cert = suboptions.at("cert");
			const std::string &key = suboptions.at("key");
			const int af = ip.find(':') == std::string::npos? AF_INET : AF_INET6;
			auto server = std::make_shared<SSLServer>(af, ip, port, cert, key, 1024);
			out.emplace_back(makeHTTP(server, suboptions));
		}

		return out;
	}
}
