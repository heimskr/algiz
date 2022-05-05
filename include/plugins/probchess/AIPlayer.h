#pragma once

#include <set>

#include "plugins/probchess/Board.h"
#include "plugins/probchess/Move.h"
#include "plugins/probchess/Player.h"

namespace ProbChess {
	class Match;

	class AIPlayer: public Player {
		protected:
			std::list<Move> getPossibleMoves(const Match &, const std::set<int> &columns) const;

		public:
			using Player::Player;

			void send(std::string_view) override;
			virtual Move chooseMove(Match &, const std::set<int> &columns) = 0;
	};
}
