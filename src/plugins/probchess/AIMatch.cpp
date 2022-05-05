#include "plugins/probchess/AIPlayer.h"
#include "plugins/probchess/HumanPlayer.h"
#include "plugins/probchess/AIMatch.h"
#include "plugins/probchess/Options.h"
#include "plugins/probchess/ProbabilityChess.h"

namespace ProbChess {
	bool AIMatch::isActive() const {
		return host.has_value();
	}

	bool AIMatch::hasClient(int client_id) const {
		if (host.has_value())
			if (HumanPlayer *human_host = dynamic_cast<HumanPlayer *>(host->get()))
				return human_host->clientID == client_id;
		return false;
	}

	bool AIMatch::isReady() const {
		return host.has_value();
	}

	void AIMatch::afterMove() {
#ifdef DEBUG_SKIPS
		std::cout << "\e[1mSkip-checking loop started.\e[0m\n";
#endif

		if (noSkip) {
			invertTurn();
			sendAll(":Turn " + colorName(currentTurn));
			do roll(); while (!canMove());
		} else {
			while (true) {
				invertTurn();
				sendAll(":Turn " + colorName(currentTurn));
				roll();
				if (!canMove()) {
					sendAll(":Skip");
				} else break;
			}
		}

#ifdef DEBUG_SKIPS
		std::cout << "\e[1mSkip-checking loop ended.\e[0m\n";
#endif

		if (currentTurn != hostColor && !over) {
			Move move = dynamic_cast<AIPlayer *>(guest->get())->chooseMove(*this, columns);
			makeMove(**guest, move);
		}
	}
}
