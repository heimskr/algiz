#pragma once

#include "http/Server.h"
#include "nlohmann/json.hpp"
#include "plugins/Plugin.h"
#include "util/Util.h"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace Algiz::HTTP {
	class Client;
}

namespace Algiz::Plugins {
	class HttpFileserv: public Plugin {
		public:
			/** Largest amount to read from a file at one time. */
			size_t chunkSize = 1 << 20;

			std::optional<std::set<std::string>> hostnames;
			std::optional<std::filesystem::path> root;

			[[nodiscard]] std::string getName()        const override { return "HTTP Fileserv"; }
			[[nodiscard]] std::string getDescription() const override { return "Serves files over HTTP."; }
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			const std::filesystem::path & getRoot(const HTTP::Server &) const;
			bool hostMatches(const HTTP::Request &) const;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> getHandler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &HttpFileserv::handleGET));

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> postHandler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this,
					&HttpFileserv::handlePOST));

		private:
			Plugins::CancelableResult handleGET(HTTP::Server::HandlerArgs &, bool not_disabled);
			Plugins::CancelableResult handlePOST(HTTP::Server::HandlerArgs &, bool not_disabled);

			bool authFailed(HTTP::Server::HandlerArgs &, const std::filesystem::path &) const;
			bool findPath(std::filesystem::path &) const;
			void serveRange(HTTP::Server::HandlerArgs &, const std::filesystem::path &) const;
			void serveFull(HTTP::Server::HandlerArgs &, const std::filesystem::path &) const;

			std::vector<std::string> getDefaults() const;
	};
}
