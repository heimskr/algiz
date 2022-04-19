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
	class HttpFileserv: public Plugin {
		public:
			virtual ~HttpFileserv() {}

			std::string getName()        const override { return "HTTP Fileserv"; }
			std::string getDescription() const override { return "Serves files from the filesystem over HTTP."; }
			std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs>> handler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs>>(bind(*this, &HttpFileserv::handle));

		private:
			Plugins::CancelableResult handle(const HTTP::Server::HandlerArgs &, bool not_disabled);

			std::vector<std::string> getDefaults(const HTTP::Server &);
	};
}
