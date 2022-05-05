#pragma once

#include <string>

#include "plugins/probchess/Color.h"

namespace ProbChess {
	struct Player {
		enum class Role {Host, Guest};

		Color color;
		Role role;

		Player(Color color_, Role role_): color(color_), role(role_) {}

		virtual ~Player() {}
		virtual void send(std::string_view) = 0;
	};
}
