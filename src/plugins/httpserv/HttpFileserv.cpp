#include "http/Server.h"
#include "plugins/HttpFileserv.h"
#include "Log.h"

namespace Algiz::Plugins {
	void HttpFileserv::postinit(PluginHost *host) {
		server = dynamic_cast<HTTP::Server *>(parent = host);
		if (!server) { ERROR("Expected HTTP server as plugin host"); return; }
		
		server->handlers.insert(handler);
	}

	void HttpFileserv::cleanup(PluginHost *host) {
		server->handlers.erase(handler);
	}

	CancelableResult HttpFileserv::handle(const HTTP::Server::HandlerArgs &args, bool not_disabled) {
		auto &[server, client, path] = args;
		INFO(":) " << path);
		return Plugins::CancelableResult::Approve;
	}
}

Algiz::Plugins::HttpFileserv ext_plugin;
