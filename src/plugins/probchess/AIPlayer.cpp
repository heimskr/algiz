#include "plugins/probchess/Match.h"
#include "plugins/probchess/AIPlayer.h"

namespace ProbChess {
	std::list<Move> AIPlayer::getPossibleMoves(const Match &match, const std::set<int> &columns) const {
		std::list<Move> possibilities;
		for (const int column: columns)
			for (int row = 0; row < match.board.height; ++row) {
				auto piece = match.board.at(row, column);
				if (!piece || piece->color != match.currentTurn)
					continue;
				for (const Square &square: piece->canMoveTo())
					possibilities.emplace_back(piece->square, square);
			}
		return possibilities;
	}

	void AIPlayer::send(std::string_view) {}
}
