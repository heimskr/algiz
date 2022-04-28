#pragma once

#include "http/Server.h"
#include "plugins/Plugin.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	class Client;
	class Server;
}

namespace Algiz::Plugins {
	class Calculator: public Plugin {
		public:
			[[nodiscard]] std::string getName()        const override { return "Calculator"; }
			[[nodiscard]] std::string getDescription() const override {
				return "Calculates simple expressions over HTTP.";
			}
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> handler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &Calculator::handle));

		private:
			Plugins::CancelableResult handle(const HTTP::Server::HandlerArgs &, bool not_disabled);
	};
}
