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
	server(server_), options(options_), webRoot(getWebRoot(options_.contains("root")? options_.at("root") : "")) {
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
		} else {
			if (request.headers.contains("Connection") && request.headers.at("Connection") == "Upgrade") {
				if (request.headers.contains("Upgrade") && request.headers.at("Upgrade") == "websocket") {
					bool failed = !request.headers.contains("Sec-WebSocket-Key")
					           || !request.headers.contains("Sec-WebSocket-Version")
					           ||  request.headers.at("Sec-WebSocket-Key").size() != 24;

					if (failed) {
						send400(client);
						return;
					}

					StringVector protocols;
					if (request.headers.contains("Sec-WebSocket-Protocol"))
						protocols = split(request.headers.at("Sec-WebSocket-Protocol"), " ");

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
							response["Upgrade"] = "websocket";
							response["Connection"] = "Upgrade";
							response["Sec-WebSocket-Accept"] = base64Encode(sha1(request.headers.at("Sec-WebSocket-Key")
								+ "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
							if (!args.acceptedProtocol.empty())
								response["Sec-WebSocket-Protocol"] = args.acceptedProtocol;
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
	}

	void Server::handlePOST(Client &client, const Request &request) {
		if (!validatePath(request.path)) {
			server->send(client.id, Response(403, "Invalid path."));
			server->close(client.id);
		} else {
			HandlerArgs args {*this, client, Request(request), getParts(request.path)};
			auto [should_pass, result] = beforeMulti(args, postHandlers);
			if (result == Plugins::HandlerResult::Pass) {
				server->send(client.id, Response(501, "Unhandled request"));
				server->close(client.id);
			}
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
		if (webSocketCloseHandlers.contains(client.id))
			for (auto &fnptr: webSocketCloseHandlers.at(client.id))
				if (auto fn = fnptr.lock())
					(*fn)(*this, client);
		server->close(client.id);
	}

	void Server::send400(Client &client) {
		server->send(client.id, Response(400, "Invalid request"));
	}

	void Server::send401(Client &client, std::string_view realm) {
		Response response(401, "Unauthorized");
		response["WWW-Authenticate"] = "Basic realm=\"" + escapeQuotes(realm) + "\"";
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
		for (const auto &view: split(path.substr(1), "/", true))
			out.push_back(unescape(view));
		return out;
	}

	void Server::cleanWebSocketHandlers() {
		cleanWebSocketMessageHandlers();
		cleanWebSocketCloseHandlers();
	}

	void Server::cleanWebSocketMessageHandlers() {
		std::vector<WeakMessageHandlerPtr> handlers_to_remove;
		std::vector<int> clients_to_remove;

		clients_to_remove.reserve(webSocketMessageHandlers.size());

		for (auto &[client_id, handlers]: webSocketMessageHandlers) {
			handlers_to_remove.reserve(handlers.size());
			for (const auto &handler: handlers)
				if (handler.expired())
					handlers_to_remove.push_back(handler);
			for (const auto &handler: handlers_to_remove)
				PluginHost::erase(handlers, handler);
			handlers_to_remove.clear();
			if (handlers.empty())
				clients_to_remove.push_back(client_id);
		}

		for (int client_id: clients_to_remove)
			webSocketMessageHandlers.erase(client_id);
	}

	void Server::cleanWebSocketCloseHandlers() {
		std::vector<WeakCloseHandlerPtr> handlers_to_remove;
		std::vector<int> clients_to_remove;

		clients_to_remove.reserve(webSocketCloseHandlers.size());

		for (auto &[client_id, handlers]: webSocketCloseHandlers) {
			handlers_to_remove.reserve(handlers.size());
			for (const auto &handler: handlers)
				if (handler.expired())
					handlers_to_remove.push_back(handler);
			for (const auto &handler: handlers_to_remove)
				PluginHost::erase(handlers, handler);
			handlers_to_remove.clear();
			if (handlers.empty())
				clients_to_remove.push_back(client_id);
		}

		for (int client_id: clients_to_remove)
			webSocketCloseHandlers.erase(client_id);
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
		if (!std::filesystem::is_directory(base))
			throw std::runtime_error("Can't crawl " + base.string() + ": not a directory");

		for (const auto &entry: std::filesystem::directory_iterator(base))
			if (entry.is_directory())
				crawlConfigs(entry.path(), map);
			else if (entry.path().filename() == ".algiz")
				try {
					map.emplace(base, nlohmann::json::parse(readFile(entry.path())));
				} catch (const nlohmann::detail::parse_error &) {
					ERROR("Invalid syntax in " << entry.path());
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
