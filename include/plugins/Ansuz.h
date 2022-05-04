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

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> handler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &Ansuz::handle));

		private:
			CancelableResult handle(const HTTP::Server::HandlerArgs &, bool not_disabled);

			static CancelableResult serve(HTTP::Server &, HTTP::Client &, std::string_view, const nlohmann::json & = {},
			                              const char *mime = "text/html");

			static CancelableResult serveIndex(HTTP::Server &, HTTP::Client &client);
	};
}
