#include "Log.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Logger.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"

namespace Algiz::Plugins {
	void Logger::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).getHandlers.emplace_back(handler);
		dynamic_cast<HTTP::Server &>(*(parent = host)).postHandlers.emplace_back(handler);
	}

	void Logger::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, handler);
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).postHandlers, handler);
	}

	CancelableResult Logger::handle(const HTTP::Server::HandlerArgs &args, bool) {
		const auto &[http, client, request, parts] = args;

		if (request.path.size() <= 2048) {
			INFO('[' << http.server->id << " <- " << client.describe() << "] Received request for \"" << escapeANSI(request.path) << "\"");
		} else {
			INFO('[' << http.server->id << " <- " << client.describe() << "] Received request for a really large path...");
		}

		static const std::vector<std::pair<std::string, std::string>> pairs{
			{"Host", "host"},
			{"Range", "range"},
		};

		for (auto [upper, lower]: pairs) {
			if (auto iter = request.headers.find(lower); iter != request.headers.end()) {
				INFO(lower << ": \"" << iter->second << '"');
			} else if (auto iter = request.headers.find(upper); iter != request.headers.end()) {
				INFO(upper << ": \"" << iter->second << '"');
			} else if (lower == "host") {
				WARN("No Host header");
			}
		}

		return CancelableResult::Pass;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Logger;
}
