#include "Board.h"
#include "piece/Pawn.h"

std::list<Square> Pawn::canMoveTo() const {
	std::list<Square> out;
	Square next = square + (color == Color::Black? 1 : -1);

	if (!parent->at(next)) {
		out.push_back(next);
		if ((square.row == 1 && color == Color::Black) || (square.row == 6 && color == Color::White)) {
			Square ahead = next + (color == Color::Black? 1 : -1);
			if (!parent->at(ahead))
				out.push_back(ahead);
		}
	}

	next = next << 1;
	if (parent->at(next))
		out.push_back(next);

	next = next >> 2;
	if (parent->at(next))
		out.push_back(next);

	return filter(out);
}
