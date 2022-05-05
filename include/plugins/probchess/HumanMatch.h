#pragma once

#include "plugins/probchess/HumanPlayer.h"
#include "plugins/probchess/Match.h"

namespace ProbChess {
	class HumanMatch: public Match {
		public:
			using Match::Match;
			~HumanMatch() {}

			bool isActive() const override;
			bool hasClient(int) const override;
			bool isReady() const override;
			void afterMove() override;
	};
}
