#pragma once

#include "plugins/probchess/Match.h"

namespace ProbChess {
	class AIMatch: public Match {
		private:
			AIMatch(const decltype(parent) &parent_, const std::string &id_, bool hidden_, bool no_skip,
			        int column_count, Color host_color):
				Match(parent_, id_, hidden_, no_skip, column_count, host_color) {}

		public:
			template <typename P>
			static std::shared_ptr<AIMatch> create(const decltype(parent) &parent_, const std::string &id_,
			                                       bool hidden_, bool no_skip, int column_count, Color host_color) {
				auto *match = new AIMatch(parent_, id_, hidden_, no_skip, column_count, host_color);
				match->guest = std::make_unique<P>(otherColor(host_color), Player::Role::Guest);
				return std::shared_ptr<AIMatch>(match);
			}

			~AIMatch() {}

			bool isActive() const override;
			bool hasClient(int) const override;
			bool isReady() const override;
			void afterMove() override;
	};
}
