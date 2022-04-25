#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Logger.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz::Plugins {
	void Logger::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).handlers.push_back(handler);
	}

	void Logger::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).handlers, std::weak_ptr(handler));
	}

	CancelableResult Logger::handle(const HTTP::Server::HandlerArgs &args, bool) {
		auto &[http, client, request, parts] = args;

		if (request.path.size() <= 2048)
			INFO("Received request for \"" << request.path << "\"");
		else
			INFO("Received request for a really large path...");

		return CancelableResult::Pass;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Logger;
}
