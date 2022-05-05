#pragma once

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "plugins/probchess/Board.h"
#include "plugins/probchess/Move.h"
#include "plugins/probchess/Player.h"

namespace Algiz::Plugins {
	class ProbabilityChess;
}

namespace ProbChess {
	class Match {
		public:
			std::weak_ptr<Algiz::Plugins::ProbabilityChess> parent;
			const std::string matchID;
			bool hidden, noSkip;
			std::optional<std::unique_ptr<Player>> host, guest;
			Color currentTurn = Color::White;
			Color hostColor;
			Board board;
			std::optional<Color> winnerColor;
			int columnCount;
			std::set<int> columns;
			bool started = false, over = false;
			std::list<std::shared_ptr<Piece>> captured;
			std::list<int> spectators;

			Match(const decltype(parent) &parent_, const std::string &match_id, bool hidden_, bool no_skip,
			      int column_count, Color host_color);
			Match(const Match &) = delete;
			Match(Match &&) = delete;

			Match & operator=(const Match &) = delete;
			Match & operator=(Match &&) = delete;

			virtual ~Match() {}

			void roll();
			virtual void end(Player *winner);
			virtual void disconnect(int client_id);
			Player & getPlayer(int client_id);
			virtual bool isActive() const = 0;
			virtual bool hasClient(int id) const = 0;
			Player & currentPlayer();
			void makeMove(Player &, const Move &);
			virtual void afterMove() = 0;
			std::list<Square> checkPawns();
			bool canMove() const;
			bool anyCanMove() const;
			bool sendHost(std::string_view);
			bool sendGuest(std::string_view);
			virtual void sendBoth(std::string_view);
			virtual void sendAll(std::string_view);
			virtual bool isReady() const = 0;
			void sendSpectators(std::string_view);
			void sendBoard();
			void invertTurn();
			std::string capturedMessage(std::shared_ptr<Piece>);
			std::string columnMessage();
			Player & getWhite();
			Player & getBlack();
			Player & get(Color);

		private:
			std::shared_ptr<Algiz::Plugins::ProbabilityChess> getParent() const;
	};
}
