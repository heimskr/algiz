#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/HttpFileserv.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz::Plugins {
	void HttpFileserv::postinit(PluginHost *host) {
		server = dynamic_cast<HTTP::Server *>(parent = host);
		if (!server) { ERROR("Expected HTTP server as plugin host"); return; }
		
		server->handlers.push_back(handler);
	}

	void HttpFileserv::cleanup(PluginHost *) {
		PluginHost::erase(server->handlers, std::weak_ptr(handler));
	}

	CancelableResult HttpFileserv::handle(const HTTP::Server::HandlerArgs &args, bool non_disabled) {
		if (!non_disabled)
			return CancelableResult::Pass;

		auto &[server, client, path] = args;
		const auto full_path = (server.webRoot / ("./" + path)).lexically_normal();

		if (!isSubpath(server.webRoot, full_path)) {
			ERROR("Not subpath of " << server.webRoot << ": " << full_path);
			return CancelableResult::Pass;
		}

		if (std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path)) {
			try {
				const std::string mime = getMIME(full_path.extension());
				server.send(client.id, HTTP::Response(200, readFile(full_path)).setMIME(mime), true);
				server.removeClient(client.id);
				return CancelableResult::Approve;
			} catch (std::exception &err) {
				ERROR(err.what());
				server.send(client.id, HTTP::Response(403, "Forbidden"), true);
				server.removeClient(client.id);
				return CancelableResult::Kill;
			}
		}

		return CancelableResult::Pass;
	}
}

Algiz::Plugins::HttpFileserv ext_plugin;
