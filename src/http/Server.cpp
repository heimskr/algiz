#include <cctype>

#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "util/Base64.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/SHA1.h"
#include "util/Util.h"

#include "Log.h"

// #define CATCH_WEBSOCKET

namespace Algiz::HTTP {
	Server::Server(const std::shared_ptr<Algiz::Server> &server_, const nlohmann::json &options_):
		server(server_),
		options(options_),
		webRoot(getWebRoot(options.contains("root")? options.at("root") : "")) {
			server->addClient = [this](auto &, int new_client, std::string_view ip) {
				auto http_client = std::make_unique<Client>(*this, new_client, ip);
				server->getClients().try_emplace(new_client, std::move(http_client));
			};

			server->closeHandler = [this](int client_id) {
				closeWebSocket(dynamic_cast<Client &>(*server->getClients().at(client_id)));
			};

			auto crawled = crawlConfigs(webRoot);
			{
				auto lock = lockConfigs();
				configs = std::move(crawled);
			}

			watcher.emplace({webRoot.string()}, true);

			watcher->onModify = [this](const std::filesystem::path &path) {
				if (path.filename() == ".algiz")
					addConfig(path);
			};

			watcherThread = std::thread([this] {
				watcher->start();
			});

			watcherThread.detach();
		}

	Server::~Server() {
		watcher->stop();
		server->messageHandler = {};
	}

	std::filesystem::path Server::getWebRoot(const std::string &web_root) {
		return std::filesystem::absolute(web_root.empty()? "./www" : web_root).lexically_normal();
	}

	bool Server::validatePath(const std::string_view &path) {
		return !path.empty() && path.front() == '/';
	}

	void Server::run() {
		server->run();
	}

	void Server::stop() {
		server->stop();
	}

	void Server::handleGET(Client &client, const Request &request) {
		if (!validatePath(request.path)) {
			server->send(client.id, Response(403, "Invalid path."));
			server->close(client.id);
			return;
		}

		if (auto iter = request.headers.find("connection"); iter != request.headers.end()) {
			const auto &header = iter->second;
			if (header == "Upgrade" || header == "keep-alive, Upgrade") {
				if (request.headers.contains("upgrade") && request.headers.at("upgrade") == "websocket") {
					bool failed = !request.headers.contains("sec-websocket-key")
					           || !request.headers.contains("sec-websocket-version")
					           ||  request.headers.at("sec-websocket-key").size() != 24;

					if (failed) {
						send400(client);
						return;
					}

					StringVector protocols;

					if (request.headers.contains("sec-websocket-protocol")) {
						protocols = split(request.headers.at("sec-websocket-protocol"), " ");
					}

					WebSocketConnectionArgs args {
						*this, client, Request(request), getParts(request.path), std::move(protocols)
					};
					client.isWebSocket = true;
					client.webSocketPath = args.parts;
					client.lineMode = false;

#ifdef CATCH_WEBSOCKET
					try {
#endif
						auto [should_pass, result] = beforeMulti(args, webSocketConnectionHandlers);
						if (result == Plugins::HandlerResult::Pass) {
							server->send(client.id, Response(501, "Unhandled request"));
							server->close(client.id);
						} else {
							Response response(101, "");
							response["upgrade"] = "websocket";
							response["connection"] = "Upgrade";
							response["sec-websocket-accept"] = base64Encode(sha1(request.headers.at("sec-websocket-key")
								+ "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

							if (!args.acceptedProtocol.empty()) {
								response["sec-websocket-protocol"] = args.acceptedProtocol;
							}

							server->send(client.id, response);
						}
#ifdef CATCH_WEBSOCKET
					} catch (const std::exception &err) {
						ERROR(err.what());
						send500(client);
						server->close(client.id);
					}
#endif

					return;
				}
			}
		}

#ifdef CATCH_WEBSOCKET
		try {
#endif
			HandlerArgs args {*this, client, Request(request), getParts(request.path)};
			auto [should_pass, result] = beforeMulti(args, getHandlers);
			if (result == Plugins::HandlerResult::Pass) {
				server->send(client.id, Response(501, "Unhandled request"));
				server->close(client.id);
			}
#ifdef CATCH_WEBSOCKET
		} catch (const std::exception &err) {
			ERROR(err.what());
			send500(client);
			server->close(client.id);
		}
#endif
	}

	void Server::handlePOST(Client &client, const Request &request) {
		if (!validatePath(request.path)) {
			server->send(client.id, Response(403, "Invalid path."));
			server->close(client.id);
			return;
		}

		HandlerArgs args(*this, client, Request(request), getParts(request.path));
		auto [should_pass, result] = beforeMulti(args, postHandlers);
		if (result == Plugins::HandlerResult::Pass) {
			server->send(client.id, Response(501, "Unhandled request"));
			server->close(client.id);
		}
	}

	void Server::handleWebSocketMessage(Client &client, std::string_view message) {
		if (webSocketMessageHandlers.contains(client.id)) {
#ifdef CATCH_WEBSOCKET
			try {
#endif
				WebSocketMessageArgs args {*this, client, message};
				beforeMulti(args, webSocketMessageHandlers.at(client.id));
#ifdef CATCH_WEBSOCKET
			} catch (const std::exception &err) {
				ERROR(err.what());
				send500(client);
				closeWebSocket(client);
			}
#endif
		}
	}

	void Server::closeWebSocket(Client &client) {
		if (webSocketCloseHandlers.contains(client.id)) {
			for (auto &fnptr: webSocketCloseHandlers.at(client.id)) {
				if (auto fn = fnptr.lock()) {
					(*fn)(*this, client);
				}
			}
		}

		server->close(client.id);
	}

	void Server::send400(Client &client) {
		server->send(client.id, Response(400, "Invalid request"));
	}

	void Server::send401(Client &client, std::string_view realm) {
		Response response(401, "Unauthorized");
		response["www-authenticate"] = "Basic realm=\"" + escapeQuotes(realm) + "\"";
		server->send(client.id, response);
	}

	void Server::send401(Client &client) {
		server->send(client.id, Response(401, "Unauthorized"));
	}

	void Server::send403(Client &client) {
		server->send(client.id, Response(403, "Forbidden"));
	}

	void Server::send500(Client &client) {
		server->send(client.id, Response(500, "Internal Server Error"));
	}

	std::vector<std::string> Server::getParts(std::string_view path) {
		std::vector<std::string> out;

		for (const auto &view: split(path.substr(1), "/", true)) {
			out.push_back(unescape(view));
		}

		return out;
	}

	void Server::cleanWebSocketHandlers() {
		cleanWebSocketMessageHandlers();
		cleanWebSocketCloseHandlers();
	}

	void Server::cleanWebSocketMessageHandlers() {
		std::erase_if(webSocketMessageHandlers, [&](auto &pair) {
			auto &[client_id, handlers] = pair;
			while (PluginHost::erase(handlers, nullptr));
			return handlers.empty();
		});
	}

	void Server::cleanWebSocketCloseHandlers() {
		std::erase_if(webSocketCloseHandlers, [&](auto &pair) {
			auto &[client_id, handlers] = pair;
			while (PluginHost::erase(handlers, nullptr));
			return handlers.empty();
		});
	}

	void Server::registerWebSocketMessageHandler(const Client &client, const WeakMessageHandlerPtr &handler) {
		webSocketMessageHandlers[client.id].push_back(handler);
	}

	void Server::registerWebSocketCloseHandler(const Client &client, const WeakCloseHandlerPtr &handler) {
		webSocketCloseHandlers[client.id].push_back(handler);
	}

	decltype(Server::configs) Server::crawlConfigs(const std::filesystem::path &base) {
		decltype(configs) out;
		crawlConfigs(base, out);
		return out;
	}

	void Server::crawlConfigs(const std::filesystem::path &base, decltype(configs) &map) {
		if (!std::filesystem::is_directory(base)) {
			throw std::runtime_error("Can't crawl " + base.string() + ": not a directory");
		}

		for (const auto &entry: std::filesystem::directory_iterator(base)) {
			if (entry.is_directory()) {
				crawlConfigs(entry.path(), map);
			} else if (entry.path().filename() == ".algiz") {
				try {
					map.emplace(base, nlohmann::json::parse(readFile(entry.path())));
				} catch (const nlohmann::detail::parse_error &) {
					ERROR("Invalid syntax in " << entry.path());
				}
			}
		}
	}

	void Server::addConfig(const std::filesystem::path &path) {
		nlohmann::json json;
		try {
			json = nlohmann::json::parse(readFile(path));
		} catch (const nlohmann::detail::parse_error &) {
			ERROR("Invalid syntax in " << path);
			return;
		}
		const auto parent = path.parent_path();
		auto lock = lockConfigs();
		configs.erase(parent);
		configs.emplace(parent, std::move(json));
		INFO("Read config from " << path);
	}
}
