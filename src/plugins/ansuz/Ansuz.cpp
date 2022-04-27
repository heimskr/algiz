#include <inja.hpp>

#include "Log.h"
#include "plugins/ansuz/resources.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Ansuz.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"

namespace Algiz::Plugins {
	void Ansuz::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).handlers.push_back(handler);
	}

	void Ansuz::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).handlers, std::weak_ptr(handler));
	}

	CancelableResult Ansuz::handle(const HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		const auto &[http, client, request, parts] = args;

		if (!parts.empty() && parts.front() == "ansuz") {
			if (parts.size() == 1)
				return serveIndex(http, client);

			http.server->send(client.id, HTTP::Response(404, "Invalid path").setMIME("text/plain"));
			http.server->removeClient(client.id);
			return CancelableResult::Approve;
		}

		return CancelableResult::Pass;
	}

	CancelableResult Ansuz::serveIndex(HTTP::Server &http, HTTP::Client &client) {
		http.server->send(client.id, HTTP::Response(200, inja::render({ansuz_index_t, ansuz_index_t_len}, {
			{"foo", "bar"}
		})).setMIME("text/html"));
		http.server->removeClient(client.id);
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Ansuz;
}
