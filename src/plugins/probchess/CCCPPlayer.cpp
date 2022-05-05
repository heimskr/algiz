// The bulk of the algorithm is based on code by Tom Murphy VII, aka suckerpinch.
// https://sourceforge.net/p/tom7misc/svn/HEAD/tree/trunk/chess/

#include <vector>

#include "plugins/probchess/CCCPPlayer.h"
#include "plugins/probchess/Match.h"

namespace ProbChess {
	bool CCCPPlayer::checkSuperior(Match &match, const LabeledMove &check, const LabeledMove &alternative) const {
		// Prefer checkmate.
		if (check.isCheckmated != alternative.isCheckmated)
			return alternative.isCheckmated;

		// Prefer check.
		if (check.isInCheck != alternative.isInCheck)
			return alternative.isInCheck;

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

	int CCCPPlayer::centerDistance(int column) const {
		switch (column) {
			case 3:
			case 4:
				return 0;
			case 2:
			case 5:
				return 1;
			case 1:
			case 6:
				return 2;
			default:
				return 3;
		}
	}

	int CCCPPlayer::moveCode(const Move &move) const {
		const int source      = move.from.row * 8 + move.from.column;
		const int destination = move.to.row   * 8 + move.to.column;
		return (source * 64 + destination) * 8;
	}

	Move CCCPPlayer::chooseMove(Match &match, const std::set<int> &columns) {
		const Color other_color = otherColor(match.currentTurn);
		std::vector<LabeledMove> labeled_moves;

		std::list<Move> legal = match.board.allMoves(match.currentTurn);
		legal.remove_if([&](const Move &move) {
			return columns.count(move.from.column) == 0;
		});

		const Board old_board = match.board;

		for (const Move &move: legal) {
			LabeledMove labeled_move(move);

			std::shared_ptr<Piece> piece = match.board.at(move.to);
			if (piece)
				labeled_move.captured = piece;

			match.board.move(match.board.at(move.from), move.to);
			labeled_move.isInCheck = match.board.isInCheck(other_color);
			labeled_move.isCheckmated = match.board.isCheckmated(other_color);
			labeled_move.isSelfInCheck = match.board.isInCheck(match.currentTurn);

			match.board = old_board;
			labeled_moves.push_back(labeled_move);
		}

		if (labeled_moves.empty())
			throw std::runtime_error("No labeled moves found.");

		LabeledMove *best = &labeled_moves.front();
		for (auto iter = std::next(labeled_moves.begin(), 1), end = labeled_moves.end(); iter != end; ++iter) {
			if (checkSuperior(match, *best, *iter))
				best = &*iter;
		}

		return best->move;
	}
}
