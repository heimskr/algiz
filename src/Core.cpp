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
		if (!server) {
			throw std::runtime_error("Server argument to makeHTTP must not be null");
		}

		auto *http = new HTTP::Server(std::move(server), suboptions);

		try {
			INFO("Binding to " << suboptions.at("ip").get<std::string>() << " on port " << suboptions.at("port") << '.');

			if (auto iter = suboptions.find("plugins"); iter != suboptions.end()) {
				for (const auto &[key, value]: iter->items()) {
					if (value.is_string()) {
						http->loadPlugin("plugin/" + value.get<std::string>() + SHARED_SUFFIX);
					} else {
						auto [path, plugin, object] = http->loadPlugin("plugin/" + value.at(0).get<std::string>() + SHARED_SUFFIX);
						plugin->setConfig(value.at(1));
					}
				}
				http->preinitPlugins();
			}

			http->server->messageHandler = [server = http->server](GenericClient &client, std::string_view message) {
				try {
					client.handleInput(message);
				} catch (const ParseError &error) {
					server->send(client.id, HTTP::Response(400, "Hey dummy, learn to send a proper HTTP request and then we can talk."));
					server->close(client.id);
				}
			};
		} catch (const std::exception &err) {
			ERROR('[' << http->server->id << "] Couldn't configure HTTP server: " << err.what());
			delete http;
			throw;
		} catch (...) {
			ERROR('[' << http->server->id << "] Couldn't configure HTTP server");
			delete http;
			throw;
		}

		return http;
	}

	void Core::run(nlohmann::json &json) {
		if (!servers.empty()) {
			throw std::runtime_error("Can't run: servers already present");
		}

		servers.reserve(2);

		std::cerr << braille;

		if (auto iter = json.find("http"); iter != json.end()) {
			const auto &suboptions = *iter;
			const std::string &ip = suboptions.at("ip");
			const uint16_t port = suboptions.at("port");
			const int af = ip.find(':') == std::string::npos? AF_INET : AF_INET6;
			const size_t threads = suboptions.contains("threads")? suboptions.at("threads").get<size_t>() : DEFAULT_THREAD_COUNT;
			auto server = std::make_unique<Server>(*this, af, ip, port, threads, 1024);
			server->id = "http";
			servers.emplace_back(makeHTTP(std::move(server), suboptions));
		}

		if (auto iter = json.find("https"); iter != json.end()) {
			const auto &suboptions = *iter;
			const uint16_t port = suboptions.at("port");
			const std::string &ip = suboptions.at("ip");
			std::string cert, key, chain;
			if (auto iter = suboptions.find("cert"); iter != suboptions.end()) {
				cert = *iter;
			}
			if (auto iter = suboptions.find("key"); iter != suboptions.end()) {
				key = *iter;
			}
			if (auto iter = suboptions.find("chain"); iter != suboptions.end()) {
				chain = *iter;
			}
			const int af = ip.find(':') == std::string::npos? AF_INET : AF_INET6;
			const size_t threads = suboptions.contains("threads")? suboptions.at("threads").get<size_t>() : DEFAULT_THREAD_COUNT;
			auto server = std::make_unique<SSLServer>(*this, af, ip, port, cert, key, chain, threads, 1024);
			server->id = "https";
			servers.emplace_back(makeHTTP(std::move(server), suboptions));
		}

		for (ApplicationServer *server: servers) {
			if (auto *host = dynamic_cast<Plugins::PluginHost *>(server)) {
				host->postinitPlugins();
			}
		}
	}

	std::shared_ptr<SSLServer> Core::getSSLServer() const {
		for (ApplicationServer *server: servers) {
			if (auto *http = dynamic_cast<HTTP::Server *>(server)) {
				if (auto ssl = std::dynamic_pointer_cast<SSLServer>(http->server)) {
					return ssl;
				}
			}
		}

		return nullptr;
	}
}
