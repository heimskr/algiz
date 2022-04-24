#include <cctype>

#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz::HTTP {
	Server::Server(const std::shared_ptr<Algiz::Server> &server_, const nlohmann::json &options_):
	server(server_), options(options_), webRoot(getWebRoot(options_.contains("root")? options_.at("root") : "")) {
		server->addClient = [this](int new_client) {
			auto http_client = std::make_unique<HTTP::Client>(*this, new_client);
			server->getClients().try_emplace(new_client, std::move(http_client));
		};
	}

	Server::~Server() {
		server->messageHandler = {};
	}

	std::filesystem::path Server::getWebRoot(const std::string &web_root) const {
		return std::filesystem::absolute(web_root.empty()? "./www" : web_root).lexically_normal();
	}

	bool Server::validatePath(const std::string_view &path) const {
		return !path.empty() && path.front() == '/';
	}

	void Server::run() {
		server->run();
	}

	void Server::stop() {
		server->stop();
	}

	void Server::handleGet(HTTP::Client &client, const Request &request) {
		if (!validatePath(request.path)) {
			server->send(client.id, Response(403, "Invalid path."));
			server->removeClient(client.id);
		} else {
			HandlerArgs args {*this, client, Request(request), getParts(request.path)};
			auto [should_pass, result] = beforeMulti(args, handlers);
			if (result == Plugins::HandlerResult::Pass) {
				server->send(client.id, Response(501, "Unhandled request"));
				server->removeClient(client.id);
			}
		}
	}

	void Server::send400(HTTP::Client &client) {
		server->send(client.id, Response(400, "Invalid request").setMIME("text/html"));
	}

	void Server::send403(HTTP::Client &client) {
		server->send(client.id, Response(403, "Forbidden").setMIME("text/html"));
	}

	std::vector<std::string> Server::getParts(const std::string_view &path) const {
		std::vector<std::string> out;
		for (const auto &view: split(path.substr(1), "/", true))
			out.push_back(unescape(view));
		return out;
	}
}
