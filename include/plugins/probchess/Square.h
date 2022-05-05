#pragma once

#include <ostream>
#include <string>
#include <utility>

#include "plugins/probchess/Color.h"

namespace ProbChess {
	class Square {
		public:
			int row, column;

			Square(int row_, int column_): row(row_), column(column_) {}

			Color getColor() const;
			bool operator==(const Square &other) const;
			bool operator!=(const Square &other) const;
			bool operator!() const;
			Square operator+(int row_shift) const;
			Square operator-(int row_shift) const;
			Square operator<<(int column_shift) const;
			Square operator>>(int column_shift) const;
			Square & operator+=(int row_shift);
			Square & operator-=(int row_shift);
			Square & operator<<=(int column_shift);
			Square & operator>>=(int column_shift);
			operator bool() const;
			operator std::string() const;
			std::string algebraic() const;
			Square operator+(const std::pair<int, int> &offsets) const;
			Square & operator+=(const std::pair<int, int> &offsets);
	};
}

std::ostream & operator<<(std::ostream &, const ProbChess::Square &);
