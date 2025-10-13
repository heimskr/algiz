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
		server.webSocketConnectionHandlers.emplace_back(connectionHandler);
	}

	void WSEcho::cleanup(PluginHost *host) {
		auto &server = dynamic_cast<HTTP::Server &>(*host);
		webSocketMessageHandlers.clear();
		PluginHost::erase(server.webSocketConnectionHandlers, connectionHandler);
		server.cleanWebSocketHandlers();
	}

	CancelableResult WSEcho::handleConnect(HTTP::Server::WebSocketConnectionArgs &args, bool not_disabled) {
		if (!not_disabled) {
			return CancelableResult::Pass;
		}

		if (args.protocols.contains("echo")) {
			args.acceptedProtocol = "echo";
			auto handler = std::make_shared<HTTP::Server::MessageHandler>(bind(*this, &WSEcho::handleMessage));
			webSocketMessageHandlers.push_back(handler);
			args.server.registerWebSocketMessageHandler(args.client, std::weak_ptr(handler));
			return CancelableResult::Approve;
		}

		return CancelableResult::Pass;
	}

	CancelableResult WSEcho::handleMessage(HTTP::Server::WebSocketMessageArgs &args, bool not_disabled) {
		if (!not_disabled) {
			return CancelableResult::Pass;
		}

		args.client.sendWebSocket(args.message, false);
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::WSEcho;
}
