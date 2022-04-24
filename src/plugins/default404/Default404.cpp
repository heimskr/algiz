#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Default404.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Templates.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz::Plugins {
	void Default404::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).handlers.push_back(handler);
	}

	void Default404::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).handlers, std::weak_ptr(handler));
	}

	CancelableResult Default404::handle(const HTTP::Server::HandlerArgs &args, bool non_disabled) {
		if (!non_disabled)
			return CancelableResult::Pass;

		auto &[http, client, request, parts] = args;

		HTTP::Response response(404, "");
		response.setMIME("text/html");

		if (http.options.contains("default404")) {
			const auto &default404 = http.options.at("default404");
			if (default404.contains("file")) {
				const std::string &filename = default404.at("file");
				response.content = readFile(filename);
				if (std::filesystem::path(filename).extension() == ".t")
					response.content = renderTemplate(response.content, {
						{"path", request.path}
					});
			}
		}

		if (response.content.empty())
			response.content = "404 Not Found";

		http.server->send(client.id, response);
		http.server->removeClient(client.id);
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Default404;
}
