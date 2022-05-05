// The bulk of the algorithm is based on code by Tom Murphy VII, aka suckerpinch.
// https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/

#include "plugins/probchess/CCCP2Player.h"
#include "plugins/probchess/Match.h"

namespace ProbChess {
	bool CCCP2Player::checkSuperior(Match &match, const LabeledMove &check, const LabeledMove &alternative) const {
		// Prefer checkmate.
		if (check.isCheckmated != alternative.isCheckmated)
			return alternative.isCheckmated;

		// Prefer check.
		if (check.isInCheck != alternative.isInCheck)
			return alternative.isInCheck;

		// Don't put self in check.
		if (check.isSelfInCheck != alternative.isSelfInCheck)
			return !alternative.isSelfInCheck;

		// Prefer capture, especially larger value.
		if (check.captured.has_value()) {
			if (!alternative.captured.has_value())
				return false;
			// Maybe return the value of the if-expression directly?
			if ((*check.captured)->typeValue() < (*alternative.captured)->typeValue())
				return true;
		} else if (alternative.captured.has_value())
			return true;

		// Prefer move depth.
		if (check.move.to.row != alternative.move.to.row) {
			if (match.currentTurn == Color::Black)
				return check.move.to.row <= alternative.move.to.row;
			return alternative.move.to.row <= check.move.to.row;
		}

		// Prefer moving towards the center.
		if (check.move.to.column != alternative.move.to.column) {
			const int checkDistance = centerDistance(check.move.to.column);
			const int alternativeDistance = centerDistance(alternative.move.to.column);
			if (checkDistance != alternativeDistance)
				return alternativeDistance < checkDistance;
		}

		// TODO: prefer promotion.

		return moveCode(check.move) < moveCode(alternative.move);
	}
}
