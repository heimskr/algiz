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

#define EXTERNAL_RESOURCES

#ifdef EXTERNAL_RESOURCES
#define RESOURCE(a, b) readFile("./.res/ansuz/" b)
#else
#define RESOURCE(a, b) std::string_view(ansuz_##a, ansuz_##a##_len)
#endif

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
			try {
				if (parts.size() == 1)
					return serveIndex(http, client);

				http.server->send(client.id, HTTP::Response(404, "Invalid path").setMIME("text/plain"));
				http.server->removeClient(client.id);
				return CancelableResult::Approve;
			} catch (const inja::RenderError &err) {
				ERROR(err.what());
			}
		}

		return CancelableResult::Pass;
	}

	CancelableResult Ansuz::serveIndex(HTTP::Server &http, HTTP::Client &client) {
		const auto plugins = map(http.getPlugins(), [](const auto &tuple) {
			return std::get<0>(tuple);
		});

		nlohmann::json json {
			{"css", RESOURCE(css, "style.css")},
			{"plugins", plugins}
		};

		http.server->send(client.id, HTTP::Response(200, inja::render(RESOURCE(index, "index.t"),
			json)).setMIME("text/html"));
		http.server->removeClient(client.id);
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Ansuz;
}
