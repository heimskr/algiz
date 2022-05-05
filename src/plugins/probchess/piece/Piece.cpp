#include "Board.h"
#include "piece/Piece.h"

std::list<Square> & Piece::filter(std::list<Square> &squares) const {
	for (auto iter = squares.begin(); iter != squares.end();) {
		if (!*iter) {
			squares.erase(iter++);
			continue;
		}

		std::shared_ptr<Piece> other = parent->at(*iter);
		if (other && other->color == color) {
			squares.erase(iter++);
			continue;
		}

		++iter;
	}

	return squares;
}

std::ostream & operator<<(std::ostream &os, const Piece &piece) {
	return os << std::string(piece);
}
