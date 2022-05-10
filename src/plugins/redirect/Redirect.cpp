#include <inja.hpp>

#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Redirect.h"

namespace Algiz::Plugins {
	void Redirect::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).getHandlers.push_back(std::weak_ptr(handler));
		if (config.contains("base") && config.at("base").is_string()) {
			base = config.at("base");
			if (!base.empty()) {
				if (base.back() == '/')
					base.pop_back();
				return;
			}
		}

		throw std::runtime_error("Redirect plugin requires a \"base\" string parameter");
	}

	void Redirect::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, std::weak_ptr(handler));
	}

	CancelableResult Redirect::handle(HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		auto &[http, client, request, parts] = args;

		HTTP::Response response(301, "");
		response.setNoContentType();
		if (request.path.empty() || request.path.front() != '/')
			response.setHeader("Location", base + '/' + request.path);
		else
			response.setHeader("Location", base + request.path);

		client.send(response);
		client.close();
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Redirect;
}
