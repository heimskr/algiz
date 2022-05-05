#include "plugins/probchess/HumanMatch.h"
#include "plugins/probchess/HumanPlayer.h"
#include "plugins/probchess/ProbabilityChess.h"

namespace ProbChess {
	bool HumanMatch::isActive() const {
		return host.has_value() || guest.has_value();
	}

	bool HumanMatch::hasClient(int client_id) const {
		if (host.has_value())
			if (HumanPlayer *human_host = dynamic_cast<HumanPlayer *>(host->get()))
				if (human_host->clientID == client_id)
					return true;
		if (guest.has_value())
			if (HumanPlayer *human_guest = dynamic_cast<HumanPlayer *>(guest->get()))
				return human_guest->clientID == client_id;
		return false;
	}

	bool HumanMatch::isReady() const {
		return host.has_value() && guest.has_value();
	}

	void HumanMatch::afterMove() {
#ifdef DEBUG_SKIPS
		std::cerr << "\e[1mSkip-checking loop started.\e[0m\n";
#endif

		if (noSkip) {
			currentTurn = currentTurn == Color::White? Color::Black : Color::White;
			sendAll(":Turn " + colorName(currentTurn));
			do roll(); while (!canMove());
		} else {
			while (true) {
				currentTurn = currentTurn == Color::White? Color::Black : Color::White;
				sendAll(":Turn " + colorName(currentTurn));
				roll();
				if (!canMove()) {
					sendAll(":Skip");
				} else break;
			}
		}

#ifdef DEBUG_SKIPS
		std::cerr << "\e[1mSkip-checking loop ended.\e[0m\n";
#endif
	}
}
