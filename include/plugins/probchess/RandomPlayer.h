#pragma once

#include "AIPlayer.h"

namespace ProbChess {
	struct RandomPlayer: public AIPlayer {
		using AIPlayer::AIPlayer;

		Move chooseMove(Match &, const std::set<int> &columns) override;
	};
}
