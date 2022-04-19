#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "Log.h"

namespace Algiz::HTTP {
	Server::Server(const Options &options):
		Algiz::Server(options.addressFamily, options.ip, options.port, 1024), webRoot(getWebRoot(options.jsonObject)) {}

	std::filesystem::path Server::getWebRoot(const nlohmann::json &json) const {
		return std::filesystem::absolute(json.contains("root")? json.at("root") : "./www").lexically_normal();
	}

	bool Server::validatePath(const std::string &path) const {
		return !path.empty() && path.front() == '/';
	}

	void Server::addClient(int new_client) {
		auto http_client = std::make_unique<HTTP::Client>(*this, new_client);
		allClients.try_emplace(new_client, std::move(http_client));
	}

	void Server::handleGet(HTTP::Client &client, const std::string &path) {
		if (!validatePath(path)) {
			send(client.id, Response(403, "Invalid path."), true);
			removeClient(client.id);
		} else {
			HandlerArgs args {*this, client, path};
			auto [should_pass, result] = beforeMulti(args, handlers);
			if (result == Plugins::HandlerResult::Pass) {
				send(client.id, Response(501, "Unhandled request"), true);
				removeClient(client.id);
			}
		}
	}
}
