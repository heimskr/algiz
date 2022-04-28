#include "Log.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/WSEcho.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Util.h"

namespace Algiz::Plugins {
	void WSEcho::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).webSocketConnectionHandlers.insert(connectionHandler);
	}

	void WSEcho::cleanup(PluginHost *host) {
		auto &server = dynamic_cast<HTTP::Server &>(*host);
		webSocketHandlers.clear();
		server.webSocketConnectionHandlers.erase(connectionHandler);
		server.cleanWebSocketHandlers();
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::WSEcho;
}
