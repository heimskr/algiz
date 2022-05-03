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

namespace Algiz::HTTP {
	Server::Server(const std::shared_ptr<Algiz::Server> &server_, const nlohmann::json &options_):
	server(server_), options(options_), webRoot(getWebRoot(options_.contains("root")? options_.at("root") : "")) {
		server->addClient = [this](auto &, int new_client) {
			auto http_client = std::make_unique<Client>(*this, new_client);
			server->getClients().try_emplace(new_client, std::move(http_client));
		};
	}

	Server::~Server() {
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

	void Server::handleGet(Client &client, const Request &request) {
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

					try {
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
					} catch (const std::exception &err) {
						ERROR(err.what());
						send500(client);
						server->close(client.id);
					}

					return;
				}
			}

			try {
				HandlerArgs args {*this, client, Request(request), getParts(request.path)};
				auto [should_pass, result] = beforeMulti(args, handlers);
				if (result == Plugins::HandlerResult::Pass) {
					server->send(client.id, Response(501, "Unhandled request"));
					server->close(client.id);
				}
			} catch (const std::exception &err) {
				ERROR(err.what());
				send500(client);
				server->close(client.id);
			}
		}
	}

	void Server::handleWebSocketMessage(Client &client, std::string_view message) {
		if (webSocketMessageHandlers.contains(client.id))
			try {
				WebSocketMessageArgs args {*this, client, message};
				beforeMulti(args, webSocketMessageHandlers.at(client.id));
			} catch (const std::exception &err) {
				ERROR(err.what());
				send500(client);
				server->close(client.id);
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
		server->send(client.id, Response(400, "Invalid request").setMIME("text/html"));
	}

	void Server::send403(Client &client) {
		server->send(client.id, Response(403, "Forbidden").setMIME("text/html"));
	}

	void Server::send500(Client &client) {
		server->send(client.id, Response(500, "Internal Server Error").setMIME("text/html"));
	}

	std::vector<std::string> Server::getParts(std::string_view path) {
		std::vector<std::string> out;
		for (const auto &view: split(path.substr(1), "/", true))
			out.push_back(unescape(view));
		return out;
	}

	void Server::cleanWebSocketHandlers() {
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

	void Server::registerWebSocketMessageHandler(const Client &client, const WeakMessageHandlerPtr &handler) {
		webSocketMessageHandlers[client.id].push_back(handler);
	}

	void Server::registerWebSocketCloseHandler(const Client &client, const WeakCloseHandlerPtr &handler) {
		webSocketCloseHandlers[client.id].push_back(handler);
	}
}
