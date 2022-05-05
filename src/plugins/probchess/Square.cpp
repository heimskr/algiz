#include "plugins/probchess/Square.h"

namespace ProbChess {
	Color Square::getColor() const {
		return row % 2 == column % 2? Color::White : Color::Black;
	}

	bool Square::operator==(const Square &other) const {
		return row == other.row && column == other.column;
	}

	bool Square::operator!=(const Square &other) const {
		return row != other.row || column != other.column;
	}

	bool Square::operator!() const {
		return row < 0 || 7 < row || column < 0 || 7 < column;
	}

	Square Square::operator+(int row_shift) const {
		return Square(row + row_shift, column);
	}

	Square Square::operator-(int row_shift) const {
		return Square(row - row_shift, column);
	}

	Square Square::operator<<(int column_shift) const {
		return Square(row, column - column_shift);
	}

	Square Square::operator>>(int column_shift) const {
		return Square(row, column + column_shift);
	}

	Square & Square::operator+=(int row_shift) {
		row += row_shift;
		return *this;
	}

	Square & Square::operator-=(int row_shift) {
		row -= row_shift;
		return *this;
	}

	Square & Square::operator<<=(int column_shift) {
		column -= column_shift;
		return *this;
	}

	Square & Square::operator>>=(int column_shift) {
		column += column_shift;
		return *this;
	}

	Square::operator bool() const {
		return 0 <= row && row < 8 && 0 <= column && column < 8;
	}

	Square::operator std::string() const {
		return std::to_string(row) + std::to_string(column);
	}

	std::string Square::algebraic() const {
		return std::string(1, 'a' + column) + std::to_string(8 - row);
	}

	Square Square::operator+(const std::pair<int, int> &offsets) const {
		return Square(row + offsets.first, column + offsets.second);
	}

	Square & Square::operator+=(const std::pair<int, int> &offsets) {
		row += offsets.first;
		column += offsets.second;
		return *this;
	}
}

std::ostream & operator<<(std::ostream &os, const ProbChess::Square &square) {
	return os << square.row << square.column;
}
