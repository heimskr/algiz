#pragma once

#include "http/Server.h"
#include "plugins/Plugin.h"
#include "threading/ThreadPool.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	class Client;
	class Server;
}

namespace Algiz::Plugins {
	class Game3CI: public Plugin {
		public:
			std::string repoRoot;
			std::string builddir;
			bool force = false;

			[[nodiscard]] std::string getName()        const override { return "Game3 CI"; }
			[[nodiscard]] std::string getDescription() const override { return "Does CI stuff for Game3."; }
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> handler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &Game3CI::handle));

		private:
			ThreadPool pool{1}; // lol

			Plugins::CancelableResult handle(const HTTP::Server::HandlerArgs &, bool not_disabled);
			ThreadPool::Function makeJob(std::string commit_hash);
	};
}
