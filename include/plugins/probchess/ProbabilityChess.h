#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "http/Server.h"
#include "plugins/Plugin.h"
#include "plugins/probchess/Color.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	class Client;
}

namespace ProbChess {
	class Match;
}

namespace Algiz::Plugins {

	class ProbabilityChess: public Plugin, public std::enable_shared_from_this<ProbabilityChess> {
		public:
			std::unordered_map<std::string, std::shared_ptr<ProbChess::Match>> matchesByID;
			std::unordered_map<int, std::shared_ptr<ProbChess::Match>> matchesByClient;

			[[nodiscard]] std::string getName()        const override { return "ProbabilityChess"; }
			[[nodiscard]] std::string getDescription() const override {
				return "Provides a Probability Chess server over WebSocket.";
			}
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			void createMatch(HTTP::Client &, const std::string &id, int column_count, ProbChess::Color, bool hidden,
			                 bool noskip, const std::string &type);
			void joinMatch(HTTP::Client &, std::string_view id, bool as_spectator);
			void leaveMatch(HTTP::Client &);

			HTTP::Server::ConnectionHandlerPtr connectionHandler =
				std::make_shared<HTTP::Server::ConnectionHandler>(bind(*this, &ProbabilityChess::handleConnect));

			HTTP::Server::CloseHandlerPtr closeHandler =
				std::make_shared<HTTP::Server::CloseHandler>(bind(*this, &ProbabilityChess::handleClose));

			HTTP::Server::MessageHandlerPtr messageHandler =
				std::make_shared<HTTP::Server::MessageHandler>(bind(*this, &ProbabilityChess::handleMessage));

			void send(int client_id, std::string_view);
			void broadcast(std::string_view);

		private:
			static constexpr const char *VALID_MATCH_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234"
				"56789_";
			std::unordered_set<int> connections;
			std::unordered_set<HTTP::Client *> clients;

			CancelableResult handleConnect(HTTP::Server::WebSocketConnectionArgs &, bool not_disabled);
			CancelableResult handleMessage(HTTP::Server::WebSocketMessageArgs &, bool not_disabled);
			void handleClose(HTTP::Server &, HTTP::Client &);

			std::recursive_mutex clientsMutex;
			auto lockClients() { return std::unique_lock(clientsMutex); }
	};

}
