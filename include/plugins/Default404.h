#pragma once

#include "http/Server.h"
#include "plugins/Plugin.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	class Client;
	class Server;
}

namespace Algiz::Plugins {
	class Default404: public Plugin {
		public:
			[[nodiscard]] std::string getName()        const override { return "Default 404"; }
			[[nodiscard]] std::string getDescription() const override { return "Serves a 404 for unhandled requests."; }
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs>> handler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs>>(bind(*this, &Default404::handle));

		private:
			Plugins::CancelableResult handle(const HTTP::Server::HandlerArgs &, bool not_disabled);
	};
}
