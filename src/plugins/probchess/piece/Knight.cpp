#include "piece/Knight.h"

std::list<Square> Knight::canMoveTo() const {
	std::list<Square> out;

	// . X . X .
	// X . . . X
	// . . x . .
	// X . . . X
	// . X . X .

	out.push_back(square + std::make_pair(-2, -1));
	out.push_back(square + std::make_pair(-2,  1));
	out.push_back(square + std::make_pair(-1,  2));
	out.push_back(square + std::make_pair( 1,  2));
	out.push_back(square + std::make_pair( 2,  1));
	out.push_back(square + std::make_pair( 2, -1));
	out.push_back(square + std::make_pair( 1, -2));
	out.push_back(square + std::make_pair(-1, -2));

	return filter(out);
}
