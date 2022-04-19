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
		
		server->handlers.insert(handler);
	}

	void HttpFileserv::cleanup(PluginHost *) {
		server->handlers.erase(handler);
	}

	CancelableResult HttpFileserv::handle(const HTTP::Server::HandlerArgs &args, bool) {
		auto &[server, client, path] = args;
		if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
			try {
				server.send(client.id, HTTP::Response(200, readFile(path)).setMIME(getMIME(path.extension())), true);
				return CancelableResult::Approve;
			} catch (std::exception &err) {
				ERROR(err.what());
				server.send(client.id, HTTP::Response(403, "Forbidden"), true);
				return CancelableResult::Kill;
			}
		}

		return CancelableResult::Disable;
	}
}

Algiz::Plugins::HttpFileserv ext_plugin;
