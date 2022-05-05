#pragma once

#include <memory>
#include <string>

#include "plugins/probchess/Player.h"

namespace Algiz::Plugins {
	class ProbabilityChess;
}

namespace ProbChess {
	struct HumanPlayer: public Player {
		Algiz::Plugins::ProbabilityChess &plugin;
		int clientID;

		HumanPlayer(decltype(plugin) &, Color, Role, int client_id);

		void send(std::string_view) override;
	};
}
