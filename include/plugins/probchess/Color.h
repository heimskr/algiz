#pragma once

#include <string>

namespace ProbChess {
	enum class Color {Black, White};
	std::string colorName(Color);
	Color otherColor(Color);
}
