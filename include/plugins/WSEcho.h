#pragma once

#include <string>
#include <vector>

#include "http/Server.h"
#include "plugins/Plugin.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	class Client;
	class Server;
}

namespace Algiz::Plugins {
	class WSEcho: public Plugin {
		public:
			[[nodiscard]] std::string getName()        const override { return "WebSocket Echo"; }
			[[nodiscard]] std::string getDescription() const override { return "Echoes data received over WebSocket."; }
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::WebSocketArgs &>> connectionHandler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::WebSocketArgs &>>(bind(*this, &WSEcho::handleConnect));

			std::vector<HTTP::Server::MessageHandlerPtr> webSocketMessageHandlers;

		private:
			CancelableResult handleConnect(HTTP::Server::WebSocketArgs &, bool not_disabled);
			CancelableResult handleMessage(HTTP::Client &, std::string_view, bool not_disabled);
	};
}
