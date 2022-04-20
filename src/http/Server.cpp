#include <cctype>

#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz::HTTP {
	Server::Server(Options &options_):
		Algiz::Server(options_.addressFamily, options_.ip, options_.port, 1024),
		options(options_),
		webRoot(getWebRoot(options_.jsonObject)) {}

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
			HandlerArgs args {*this, client, std::string(path), getParts(path)};
			auto [should_pass, result] = beforeMulti(args, handlers);
			if (result == Plugins::HandlerResult::Pass) {
				send(client.id, Response(501, "Unhandled request"), true);
				removeClient(client.id);
			}
		}
	}

	std::vector<std::string> Server::getParts(const std::string &path) const {
		std::vector<std::string> out;
		for (const auto &view: split(std::string_view(path).substr(1), "/", true))
			out.push_back(unescape(view));
		return out;
	}
}
