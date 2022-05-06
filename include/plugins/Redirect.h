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
	class Redirect: public Plugin {
		public:
			[[nodiscard]] std::string getName()        const override { return "Redirect"; }
			[[nodiscard]] std::string getDescription() const override { return "Redirects all HTTP requests."; }
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> handler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &Redirect::handle));

		private:
			std::string base;

			Plugins::CancelableResult handle(HTTP::Server::HandlerArgs &, bool not_disabled);
	};
}
