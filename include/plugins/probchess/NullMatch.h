#pragma once

#include <memory>

#include "plugins/probchess/Match.h"

namespace ProbChess {
	class NullMatch: public Match {
		public:
			NullMatch(const decltype(parent) &parent_, const std::string &id_, bool hidden_, bool no_skip,
			          int column_count, Color host_color):
			Match(parent_, id_, hidden_, no_skip, column_count, host_color) {
				currentTurn = host_color;
			}

			~NullMatch() {}

			bool isActive() const override;
			bool hasClient(int) const override;
			bool isReady() const override;
			void afterMove() override;
	};
}
