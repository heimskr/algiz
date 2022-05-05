#pragma once

#include <string>

#include "plugins/probchess/Square.h"

namespace ProbChess {
	struct Move {
		Square from;
		Square to;

		Move(Square from_, Square to_): from(from_), to(to_) {}
		Move(std::string_view);

		std::string pseudoalgebraic() const;
	};
}
