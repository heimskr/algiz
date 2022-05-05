#include "plugins/probchess/NullMatch.h"
#include "plugins/probchess/HumanPlayer.h"
#include "plugins/probchess/NoKingError.h"
#include "plugins/probchess/Options.h"
#include "plugins/probchess/ProbabilityChess.h"

namespace ProbChess {
	bool NullMatch::isActive() const {
		return host.has_value();
	}

	bool NullMatch::hasClient(int client_id) const {
		if (host.has_value())
			if (HumanPlayer *human_host = dynamic_cast<HumanPlayer *>(host->get()))
				return human_host->clientID == client_id;
		return false;
	}

	bool NullMatch::isReady() const {
		return host.has_value();
	}

	void NullMatch::afterMove() {
		sendAll(":Turn " + colorName(currentTurn));
		do roll(); while (!canMove());
	}
}
