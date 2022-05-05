#include <cerrno>
#include <cstdlib>
#include <iostream>

#include "http/Client.h"
#include "plugins/probchess/AIMatch.h"
#include "plugins/probchess/Board.h"
#include "plugins/probchess/CCCPPlayer.h"
#include "plugins/probchess/CCCP2Player.h"
#include "plugins/probchess/ChessError.h"
#include "plugins/probchess/HumanMatch.h"
#include "plugins/probchess/HumanPlayer.h"
#include "plugins/probchess/Match.h"
#include "plugins/probchess/NullMatch.h"
#include "plugins/probchess/ProbabilityChess.h"
#include "plugins/probchess/RandomPlayer.h"
#include "plugins/probchess/Square.h"
#include "plugins/probchess/piece/all.h"
#include "util/Util.h"

#include "Log.h"

namespace Algiz::Plugins {
	void ProbabilityChess::postinit(PluginHost *host) {
		auto &server = dynamic_cast<HTTP::Server &>(*(parent = host));
		server.webSocketConnectionHandlers.emplace_back(connectionHandler);
	}

	void ProbabilityChess::cleanup(PluginHost *host) {
		auto &server = dynamic_cast<HTTP::Server &>(*host);
		PluginHost::erase(server.webSocketConnectionHandlers, std::weak_ptr(connectionHandler));
		server.cleanWebSocketHandlers();
	}

	CancelableResult ProbabilityChess::handleConnect(HTTP::Server::WebSocketConnectionArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		if (args.protocols.contains("probchess")) {
			args.acceptedProtocol = "probchess";
			args.server.registerWebSocketMessageHandler(args.client, std::weak_ptr(messageHandler));
			args.server.registerWebSocketCloseHandler(args.client, std::weak_ptr(closeHandler));
			connections.insert(args.client.id);
			clients.insert(&args.client);
			return CancelableResult::Approve;
		}

		return CancelableResult::Pass;
	}

	CancelableResult ProbabilityChess::handleMessage(HTTP::Server::WebSocketMessageArgs &args, bool not_disabled) {
		if (!not_disabled)
			return CancelableResult::Pass;

		auto &client = args.client;
		std::string_view message = args.message;

		if (message.empty() || message[0] != ':') {
			client.sendWebSocket(":Error Invalid message.");
			return CancelableResult::Approve;
		}

		const auto words = split(message.substr(1), " ");
		const auto verb = words[0];

		// :Create <id> <column-count> <black/white> <hidden/public> <skip/noskip> <type>
		if (verb == "Create") {
			if (words.size() != 7 || words[1].empty() || words[2].size() != 1) {
				client.sendWebSocket(":Error Invalid message.");
				return CancelableResult::Approve;
			}

			createMatch(client, std::string(words[1]), words[2][0] - '0', words[3] == "black"?
				ProbChess::Color::Black : ProbChess::Color::White, words[4] == "hidden", words[5] == "noskip",
				std::string(words[6]));
			return CancelableResult::Approve;
		}

		// :Join <id>
		if (verb == "Join") {
			if (words.size() != 2 || words[1].empty()) {
				client.sendWebSocket(":Error Invalid message.");
				return CancelableResult::Approve;
			}

			joinMatch(client, words[1], false);
			return CancelableResult::Approve;
		}

		// :Spectate <id>
		if (verb == "Spectate") {
			if (words.size() != 2 || words[1].empty()) {
				client.sendWebSocket(":Error Invalid message.");
				return CancelableResult::Approve;
			}

			joinMatch(client, words[1], true);
			return CancelableResult::Approve;
		}

		// :CreateOrJoin <id> <column-count> <black/white> <hidden/public> <skip/noskip> <type>
		if (verb == "CreateOrJoin") {
			if (words.size() != 7 || words[1].empty() || words[2].size() != 1) {
				client.sendWebSocket(":Error Invalid message.");
				return CancelableResult::Approve;
			}

			if (matchesByID.contains(std::string(words[1])))
				joinMatch(client, words[1], false);
			else
				createMatch(client, std::string(words[1]), words[2][0] - '0', words[3] == "black"?
					ProbChess::Color::Black : ProbChess::Color::White, words[4] == "hidden", words[5] == "noskip",
					std::string(words[6]));
			return CancelableResult::Approve;
		}

		// :Move <from-square> <to-square>
		if (verb == "Move") {
			if (words.size() != 3) {
				client.sendWebSocket(":Error Invalid message.");
				return CancelableResult::Approve;
			}

			for (const auto &str: {words[1], words[2]})
				if (str.size() != 2 || str.find_first_not_of("01234567") != std::string::npos) {
					client.sendWebSocket(":Error Invalid message.");
					return CancelableResult::Approve;
				}

			if (matchesByClient.count(client.id) == 0) {
				client.sendWebSocket(":Error Not in a match.");
				return CancelableResult::Approve;
			}

			auto match = matchesByClient.at(client.id);
			ProbChess::Square from {words[1][0] - '0', words[1][1] - '0'};
			ProbChess::Square to   {words[2][0] - '0', words[2][1] - '0'};

			try {
				match->makeMove(match->getPlayer(client.id), {from, to});
			} catch (ChessError &err) {
				client.sendWebSocket(":Error " + std::string(err.what()));
			}

			return CancelableResult::Approve;
		}

		// :GetMatches
		if (verb == "GetMatches") {
			client.sendWebSocket(":ClearMatches");
			for (const auto &[match_id, match]: matchesByID)
				if (!match->hidden)
					client.sendWebSocket(":Match " + match_id + " " + (match->isReady()? "closed" : "open"));
			return CancelableResult::Approve;
		}

		// :FEN
		if (verb == "FEN") {
			std::shared_ptr<ProbChess::Match> match;
			try {
				match = matchesByClient.at(client.id);
			} catch (std::out_of_range &) {
				client.sendWebSocket(":Error Not in a match.");
				return CancelableResult::Approve;
			}

			client.sendWebSocket(":FEN " + match->board.toFEN(match->currentTurn));
			return CancelableResult::Approve;
		}

		// :LeaveMatch
		if (verb == "LeaveMatch") {
			leaveMatch(client);
			return CancelableResult::Approve;
		}

		client.sendWebSocket(":Error Unknown message type");
		return CancelableResult::Approve;
	}
	
	void ProbabilityChess::handleClose(HTTP::Server &, HTTP::Client &client) {
		const int id = client.id;
		auto lock = lockClients();
		connections.erase(id);
		clients.erase(&client);
		if (matchesByClient.contains(id)) {
			matchesByClient.at(id)->disconnect(id);
			matchesByClient.erase(id);
		}
	}

	void ProbabilityChess::send(int client_id, std::string_view message) {
		auto &server = dynamic_cast<HTTP::Server &>(*parent);
		auto &client = dynamic_cast<HTTP::Client &>(*server.server->allClients.at(client_id));
		client.sendWebSocket(message);
	}

	void ProbabilityChess::broadcast(std::string_view message) {
		auto lock = lockClients();
		for (auto *client: clients)
			client->sendWebSocket(message);
	}

	void ProbabilityChess::createMatch(HTTP::Client &client, const std::string &id, int column_count,
	                                   ProbChess::Color color, bool hidden, bool noskip, const std::string &type) {
		if (id.find_first_not_of(VALID_MATCH_CHARS) != std::string::npos) {
			client.sendWebSocket(":Error Invalid match ID.");
			return;
		}

		if (column_count < 1 || 8 < column_count) {
			client.sendWebSocket(":Error Invalid column count.");
			return;
		}

		if (matchesByClient.contains(client.id)) {
			client.sendWebSocket(":Error Already in a match.");
			return;
		}

		if (matchesByID.count(id) > 0) {
			client.sendWebSocket(":Error A match with that ID already exists.");
			return;
		}

		std::shared_ptr<ProbChess::Match> match;
		if (type == "human") {
			match = std::make_shared<ProbChess::HumanMatch>(this, id, hidden, noskip, column_count, color);
		} else if (type == "random") {
			match = ProbChess::AIMatch::create<ProbChess::RandomPlayer>(this, id, hidden, noskip, column_count,
				color);
		} else if (type == "cccp") {
			match = ProbChess::AIMatch::create<ProbChess::CCCPPlayer>(this, id, hidden, noskip, column_count,
				color);
		} else if (type == "cccp2") {
			match = ProbChess::AIMatch::create<ProbChess::CCCP2Player>(this, id, hidden, noskip, column_count,
				color);
		} else if (type == "null") {
			match = std::make_shared<ProbChess::NullMatch>(this, id, hidden, noskip, column_count, color);
		} else {
			client.sendWebSocket(":Error Invalid match type.");
			return;
		}

		match->host = std::make_unique<ProbChess::HumanPlayer>(*this, color, ProbChess::Player::Role::Host, client.id);
		matchesByID.emplace(id, match);
		matchesByClient.emplace(client.id, match);
		client.sendWebSocket(":Joined " + id + " " + ProbChess::colorName(color));
		INFO("Client created " << (match->hidden? "hidden " : "") << "match \e[32m" << id << "\e[39m.");
		if (!match->hidden)
			broadcast(":Match " + match->matchID + " " + (match->isReady()? "closed" : "open"));

		if (match->isReady()) {
			client.sendWebSocket(":Start");
			match->sendAll(":Turn " + colorName(match->currentTurn));
			match->sendBoard();

			match->started = true;
			if (!match->anyCanMove())
				match->end(&(match->board.getPieces(ProbChess::Color::White).empty()?
					match->getBlack() : match->getWhite()));
			do match->roll(); while (!match->canMove());

			if (!match->hidden)
				broadcast(":Match " + match->matchID + " " + (match->isReady()? "closed" : "open"));
		} else {
			// Hack for PCS3D
			match->sendBoard();
		}
	}

	void ProbabilityChess::joinMatch(HTTP::Client &client, std::string_view id, bool as_spectator) {
		auto lock = lockClients();

		if (matchesByClient.contains(client.id)) {
			client.sendWebSocket(":Error Already in a match.");
			return;
		}

		const std::string id_string = std::string(id);

		if (!matchesByID.contains(id_string)) {
			client.sendWebSocket(":Error No match with that ID exists.");
			return;
		}

		auto match = matchesByID.at(id_string);

		if (!as_spectator && match->host.has_value() && match->guest.has_value()) {
			client.sendWebSocket(":Error Match is full.");
			return;
		}

		matchesByClient.emplace(client.id, match);
		if (as_spectator)
			client.sendWebSocket(":Joined " + id_string + " spectator");
		else if (!match->host.has_value())
			client.sendWebSocket(":Joined " + id_string + " " + colorName(match->hostColor));
		else
			client.sendWebSocket(":Joined " + id_string + " " + colorName(otherColor(match->hostColor)));

		if (as_spectator) {
			if (match->started)
				client.sendWebSocket(":Start");
		} else if (!match->started)
			match->sendAll(":Start");

		const char *as = "unknown";
		if (as_spectator) {
			match->spectators.push_back(client.id);
			as = "spectator";
		} else if (!match->host.has_value()) {
			match->host = std::make_unique<ProbChess::HumanPlayer>(*this, match->hostColor,
				ProbChess::Player::Role::Host, client.id);
			as = "host";
		} else if (!match->guest.has_value()) {
			match->guest = std::make_unique<ProbChess::HumanPlayer>(*this, otherColor(match->hostColor),
				ProbChess::Player::Role::Guest, client.id);
			as = "guest";
		}

		if (!as_spectator)
			client.sendWebSocket(":Start");

		if (match->isReady())
			match->sendAll(":Turn " + colorName(match->currentTurn));

		match->sendBoard();

		if (!as_spectator && !match->started) {
			match->started = true;
			if (!match->anyCanMove())
				match->end(&(match->board.getPieces(ProbChess::Color::White).empty()?
					match->getBlack() : match->getWhite()));
			do
				match->roll();
			while (!match->canMove());
		} else {
			client.sendWebSocket(match->columnMessage());
			for (const auto &piece: match->captured)
				client.sendWebSocket(match->capturedMessage(piece));
		}

		if (!as_spectator && !match->hidden)
			broadcast(":Match " + match->matchID + " " + (match->isReady()? "closed" : "open"));

		INFO("Client joined match \e[32m" << id << "\e[39m as \e[1m" << as << "\e[22m.");
	}

	void ProbabilityChess::leaveMatch(HTTP::Client &client) {
		auto lock = lockClients();

		if (!matchesByClient.contains(client.id)) {
			WARN("Client not in a match attempted to leave a match.");
			return;
		}

		auto match = matchesByClient.at(client.id);
		INFO("Disconnecting client " << client.id << " from match \e[33m" << match->matchID << "\e[39m.");
		match->disconnect(client.id);
		matchesByClient.erase(client.id);
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::ProbabilityChess;
}
