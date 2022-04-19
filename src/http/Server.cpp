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

	void Server::addClient(int new_client) {
		auto http_client = std::make_unique<HTTP::Client>(*this, new_client);
		allClients.try_emplace(new_client, std::move(http_client));
	}

	void Server::handleGet(HTTP::Client &client, const std::string &path) {
		const auto full_path = (webRoot / ("./" + path)).lexically_normal();

		if (!isSubpath(webRoot, full_path)) {
			send(client.id, Response(403, "Invalid path."), true);
		} else if (std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path)) {
			HandlerArgs args {*this, client, full_path};
			if (!before(args, handlers)) {
				send(client.id, Response(501, "Unhandled request"), true);
			}
			// try {
			// 	send(client.id, Response(200, readFile(full_path)).setMIME(getMIME(full_path.extension())), true);
			// } catch (std::exception &err) {
			// 	ERROR(err.what());
			// 	send(client.id, Response(403, "Forbidden"), true);
			// }
		} else {
			send(client.id, Response(404, "File not found."), true);
		}

		removeClient(client.id);
	}
}
