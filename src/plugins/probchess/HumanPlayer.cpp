#include "http/Server.h"
#include "plugins/probchess/HumanPlayer.h"
#include "plugins/probchess/ProbabilityChess.h"

namespace ProbChess {
	HumanPlayer::HumanPlayer(decltype(plugin) &plugin_, Color color_, Role role_, int client_id):
		Player(color_, role_), plugin(plugin_), clientID(client_id) {}

	void HumanPlayer::send(std::string_view message) {
		plugin.send(clientID, message);
	}
}
