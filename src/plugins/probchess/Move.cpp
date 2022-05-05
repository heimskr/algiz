#include <iostream>
#include "plugins/probchess/Move.h"

namespace ProbChess {
	Move::Move(std::string_view str): from(0, 0), to(0, 0) {
		from.column = str[0] - 'a';
		from.row = 8 - (str[1] - '0');
		to.column = str[2] - 'a';
		to.row = 8 - (str[3] - '0');
	}

	std::string Move::pseudoalgebraic() const {
		return from.algebraic() + to.algebraic();
	}
}
