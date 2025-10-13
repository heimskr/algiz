#include <inja/inja.hpp>

#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Redirect.h"

namespace Algiz::Plugins {
	void Redirect::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).getHandlers.emplace_back(handler);
		if (config.contains("base") && config.at("base").is_string()) {
			base = config.at("base");
			if (!base.empty()) {
				if (base.back() == '/') {
					base.pop_back();
				}
				return;
			}
		}

		base = "https";
	}

	void Redirect::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, handler);
	}

	CancelableResult Redirect::handle(HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		auto &[http, client, request, parts] = args;

		HTTP::Response response(301, "");
		response.setNoContentType();
		std::string modified_base;
		std::string_view base_view = base;

		if (base == "https") {
			if (auto host = args.request.getHeader("host"); !host.empty()) {
				modified_base = std::format("https://{}", host);
				base_view = modified_base;
			}
		}

		if (request.path.empty() || request.path.front() != '/') {
			response.setHeader("location", std::format("{}/{}", base_view, request.pathWithParameters()));
		} else {
			response.setHeader("location", std::format("{}{}", base_view, request.pathWithParameters()));
		}

		client.send(response);
		client.close();
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Redirect;
}
