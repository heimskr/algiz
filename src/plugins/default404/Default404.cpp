#include "Log.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Default404.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Templates.h"
#include "util/Util.h"

namespace Algiz::Plugins {
	void Default404::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).getHandlers.emplace_back(handler);
	}

	void Default404::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, handler);
	}

	CancelableResult Default404::handle(const HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		const auto &[http, client, request, parts] = args;

		HTTP::Response response(404, "");
		response.setMIME("text/html");

		if (config.contains("file")) {
			const std::string &filename = config.at("file");
			response.content = readFile(filename);
			if (std::filesystem::path(filename).extension() == ".t")
				response.content = renderTemplate(response.contentView(), {
					{"path", request.path}
				});
		}

		if (response.contentView().empty())
			response.content = std::string("404 Not Found");

		http.server->send(client.id, response);
		http.server->close(client.id);
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Default404;
}
