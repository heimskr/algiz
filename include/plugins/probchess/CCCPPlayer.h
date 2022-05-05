#pragma once

// The bulk of the algorithm is based on code by Tom Murphy VII, aka suckerpinch.
// https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/

#include <optional>

#include "plugins/probchess/AIPlayer.h"

namespace ProbChess {
	class CCCPPlayer: public AIPlayer {
		protected:
			struct LabeledMove {
				Move move;
				bool isCheckmated = false, isInCheck = false, isSelfInCheck = false;
				std::optional<std::shared_ptr<Piece>> captured;
				LabeledMove(const Move &move_): move(move_) {}
				LabeledMove(const Move &move_, bool is_checkmated, bool is_in_check, bool is_self_in_check):
					move(move_), isCheckmated(is_checkmated), isInCheck(is_in_check), isSelfInCheck(is_self_in_check) {}
				LabeledMove(const Move &move_, bool is_checkmated, bool is_in_check, bool is_self_in_check,
							const std::shared_ptr<Piece> &captured_):
					move(move_), isCheckmated(is_checkmated), isInCheck(is_in_check), isSelfInCheck(is_self_in_check),
					captured(captured_) {}
			};

			/** Checks whether the second given move is better than the first given move. */
			virtual bool checkSuperior(Match &, const LabeledMove &check, const LabeledMove &alternative) const;

			int centerDistance(int column) const;
			int moveCode(const Move &) const;

		public:
			using AIPlayer::AIPlayer;

			Move chooseMove(Match &, const std::set<int> &columns) override;
	};
}
