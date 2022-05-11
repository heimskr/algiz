#pragma once

#include "http/Server.h"
#include "plugins/Plugin.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	class Client;
	class Server;
}

namespace Algiz::Plugins {
	class Ansuz: public Plugin {
		public:
			[[nodiscard]] std::string getName()        const override { return "Ansuz"; }
			[[nodiscard]] std::string getDescription() const override {
				return "Provides a control panel for Algiz over HTTP.";
			}
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> getHandler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &Ansuz::handleGET));

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> postHandler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &Ansuz::handlePOST));

		private:
			CancelableResult handleGET(HTTP::Server::HandlerArgs &, bool not_disabled);
			CancelableResult handlePOST(HTTP::Server::HandlerArgs &, bool not_disabled);

			/** Returns whether authentication succeeded. */
			bool checkAuth(HTTP::Server &, HTTP::Client &, const HTTP::Request &);

			static CancelableResult serve(HTTP::Server &, HTTP::Client &, std::string_view, const nlohmann::json & = {},
			                              int code = 200, const char *mime = "text/html");

			static CancelableResult serveIndex(HTTP::Server &, HTTP::Client &client);
	};
}
