#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "http/Server.h"
#include "nlohmann/json.hpp"
#include "plugins/Plugin.h"
#include "util/Util.h"
#include "wahtwo/Watcher.h"

namespace Algiz::HTTP {
	class Client;
	class Server;
}

namespace Algiz::Plugins {
	class HttpFileserv: public Plugin {
		public:
			/** Largest amount to read from a file at one time. */
			size_t chunkSize = 1 << 24;

			[[nodiscard]] std::string getName()        const override { return "HTTP Fileserv"; }
			[[nodiscard]] std::string getDescription() const override {
				return "Serves files from the filesystem over HTTP.";
			}
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> handler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &HttpFileserv::handle));

		private:
			std::optional<Wahtwo::Watcher> watcher;
			std::thread watcherThread;
			std::map<std::filesystem::path, nlohmann::json> configs;
			std::mutex configsMutex;

			Plugins::CancelableResult handle(HTTP::Server::HandlerArgs &, bool not_disabled);

			std::vector<std::string> getDefaults();
			static decltype(configs) crawlConfigs(const std::filesystem::path &base);
			static void crawlConfigs(const std::filesystem::path &base, decltype(configs) &);
			void addConfig(const std::filesystem::path &);

			auto lockConfigs() { return std::unique_lock(configsMutex); }
	};
}
