#include "plugins/probchess/piece/King.h"

namespace ProbChess {
	std::list<Square> King::canMoveTo() const {
		std::list<Square> out;

		out.push_back(square + std::make_pair(-1,  0));
		out.push_back(square + std::make_pair(-1,  1));
		out.push_back(square + std::make_pair( 0,  1));
		out.push_back(square + std::make_pair( 1,  1));
		out.push_back(square + std::make_pair( 1,  0));
		out.push_back(square + std::make_pair( 1, -1));
		out.push_back(square + std::make_pair( 0, -1));
		out.push_back(square + std::make_pair(-1, -1));

		return filter(out);
	}
}
