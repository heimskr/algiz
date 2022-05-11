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
		try {
			INFO("Binding to " << suboptions.at("ip").get<std::string>() << " on port " << suboptions.at("port")
				<< ".");

			if (suboptions.contains("plugins")) {
				for (const auto &[key, value]: suboptions.at("plugins").items()) {
					if (value.is_string()) {
						http->loadPlugin("plugin/" + value.get<std::string>() + SHARED_SUFFIX);
					} else {
						auto [path, plugin, object] =
							http->loadPlugin("plugin/" + value.at(0).get<std::string>() + SHARED_SUFFIX);
						plugin->setConfig(value.at(1));
					}
				}
				http->preinitPlugins();
				http->postinitPlugins();
			}

			http->server->messageHandler = [server = http->server](GenericClient &client, std::string_view message) {
				try {
					client.handleInput(message);
				} catch (const ParseError &error) {
					ERROR("[" << server->id << "] Disconnecting client " << client.id << ": " << error.what());
					server->send(client.id, HTTP::Response(400, "Couldn't parse request."));
					server->close(client.id);
				}
			};
		} catch (const std::exception &err) {
			ERROR("[" << server->id << "] Couldn't configure HTTP server: " << err.what());
			delete http;
			throw err;
		}

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
			const size_t threads = suboptions.contains("threads")?
				suboptions.at("threads").get<size_t>() : DEFAULT_THREAD_COUNT;
			auto server = std::make_unique<Server>(af, ip, port, threads, 1024);
			server->id = "http";
			out.emplace_back(makeHTTP(std::move(server), suboptions));
		}

		if (json.contains("https")) {
			const auto &suboptions = json.at("https");
			const std::string &ip = suboptions.at("ip");
			const uint16_t port = suboptions.at("port");
			const std::string &cert = suboptions.at("cert");
			const std::string &key = suboptions.at("key");
			const int af = ip.find(':') == std::string::npos? AF_INET : AF_INET6;
			const size_t threads = suboptions.contains("threads")?
				suboptions.at("threads").get<size_t>() : DEFAULT_THREAD_COUNT;
			auto server = std::make_unique<SSLServer>(af, ip, port, cert, key, threads, 1024);
			server->id = "https";
			out.emplace_back(makeHTTP(std::move(server), suboptions));
		}

		return out;
	}
}
