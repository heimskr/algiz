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
		auto &server = dynamic_cast<HTTP::Server &>(*(parent = host));
		server.webSocketConnectionHandlers.insert(std::weak_ptr(connectionHandler));
	}

	void WSEcho::cleanup(PluginHost *host) {
		auto &server = dynamic_cast<HTTP::Server &>(*host);
		webSocketMessageHandlers.clear();
		server.webSocketConnectionHandlers.erase(std::weak_ptr(connectionHandler));
		server.cleanWebSocketHandlers();
	}

	Plugins::CancelableResult WSEcho::handle(HTTP::Server::WebSocketArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		if (args.protocols.contains("echo")) {
			args.acceptedProtocol = "echo";
			auto handler = std::make_shared<HTTP::Server::MessageHandler>([](std::string_view, bool) {

				return CancelableResult::Approve;
			});
			webSocketMessageHandlers.push_back(handler);
			args.server.registerWebSocketMessageHandler(args.client, std::weak_ptr(handler));

			return CancelableResult::Approve;
		}

		return CancelableResult::Pass;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::WSEcho;
}
