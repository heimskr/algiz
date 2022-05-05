#include <cstdlib>
#include <iostream>

#include "plugins/probchess/Match.h"
#include "plugins/probchess/RandomPlayer.h"

namespace ProbChess {
	Move RandomPlayer::chooseMove(Match &match, const std::set<int> &columns) {
		std::list<Move> possibilities = getPossibleMoves(match, columns);

		if (possibilities.empty())
			throw std::runtime_error("RandomBot has no moves to select from.");

		const int index = rand() % possibilities.size();
		std::cerr << "Choosing possibility \e[1m" << index << "\e[22m out of \e[1m" << possibilities.size()
		          << "\e[22m.\n";
		return *std::next(possibilities.begin(), index);
	}
}
