#include <functional>
#include <iostream>
#include <sstream>

#include "plugins/probchess/Board.h"
#include "plugins/probchess/NoKingError.h"
#include "plugins/probchess/piece/all.h"
#include "util/Util.h"

namespace ProbChess {
	Board::Board(const Board &other) {
		width = other.width;
		height = other.height;
		for (int row = 0; row < height; ++row)
			for (int column = 0; column < width; ++column) {
				const std::shared_ptr<Piece> piece = other.at(row, column);
				if (piece)
					pieces[row][column] = std::shared_ptr<Piece>(piece->clone(this));
			}
	}

	Board & Board::operator=(const Board &other) {
		width = other.width;
		height = other.height;
		for (int row = 0; row < height; ++row)
			for (int column = 0; column < width; ++column) {
				const std::shared_ptr<Piece> piece = other.at(row, column);
				if (piece)
					pieces[row][column] = std::shared_ptr<Piece>(piece->clone(this));
				else
					pieces[row][column].reset();
			}
		return *this;
	}

	std::shared_ptr<Piece> Board::at(int row, int column) const {
		if (row < 0 || height <= row || column < 0 || width <= column)
			return nullptr;
		return pieces[row][column];
	}

	std::shared_ptr<Piece> Board::at(Square square) const {
		return at(square.row, square.column);
	}

	bool Board::isAttacked(Color color, const Square &square) {
		std::shared_ptr<Piece> old_piece = at(square);
		std::shared_ptr<Rook>  new_piece = std::make_shared<Rook>(this, otherColor(color), square);

		pieces[square.row][square.column] = new_piece;

		for (int row = 0; row < height; ++row)
			for (int column = 0; column < width; ++column) {
				std::shared_ptr<Piece> piece = at(row, column);
				if (!piece || piece->color != color)
					continue;
				for (const Square &move: piece->canMoveTo())
					if (move == square) {
						pieces[square.row][square.column] = old_piece;
						return true;
					}
			}

		pieces[square.row][square.column] = old_piece;
		return false;
	}

	std::shared_ptr<Piece> Board::findKing(Color color) const {
		for (int row = 0; row < height; ++row) {
			for (int column = 0; column < width; ++column) {
				std::shared_ptr<Piece> piece = at(row, column);
				if (piece && piece->color == color && piece->getType() == Piece::Type::King)
					return piece;
			}
		}

		throw NoKingError("Couldn't find " + colorName(color) + " king.");
	}

	bool Board::isInCheck(Color color) {
		try {
			return isAttacked(color, findKing(color)->square);
		} catch (const NoKingError &) {
			return false;
		}
	}

	bool Board::isCheckmated(Color color) {
		std::shared_ptr<Piece> king;
		try {
			king = findKing(color);
		} catch (const NoKingError &) {
			return true;
		}

		if (!isAttacked(color, king->square))
			return false;

		const Board old_board = *this;
		const Square old_square = king->square; 

		for (const Square &square: king->canMoveTo()) {
			move(king, square);
			if (!isAttacked(color, king->square))
				return false;

			*this = old_board;
			king->square = old_square;
		}

		return true;
	}

	std::list<Move> Board::allMoves() const {
		std::list<Move> out;
		for (int row = 0; row < height; ++row)
			for (int column = 0; column < width; ++column) {
				std::shared_ptr<Piece> piece = at(row, column);
				if (piece)
					for (const Square &to: piece->canMoveTo())
						out.push_back({piece->square, to});
			}
		return out;
	}

	std::list<Move> Board::allMoves(Color color) const {
		std::list<Move> out;
		for (int row = 0; row < height; ++row)
			for (int column = 0; column < width; ++column) {
				std::shared_ptr<Piece> piece = at(row, column);
				if (piece && piece->color == color)
					for (const Square &to: piece->canMoveTo())
						out.push_back({piece->square, to});
			}
		return out;
	}

	void Board::placePieces() {
		for (int row = 0; row < height; ++row)
			for (int column = 0; column < width; ++column)
				pieces[row][column].reset();
		
		set<Rook>(Color::Black,   0, 0);
		set<Knight>(Color::Black, 0, 1);
		set<Bishop>(Color::Black, 0, 2);
		set<Queen>(Color::Black,  0, 3);
		set<King>(Color::Black,   0, 4);
		set<Bishop>(Color::Black, 0, 5);
		set<Knight>(Color::Black, 0, 6);
		set<Rook>(Color::Black,   0, 7);

		set<Rook>(Color::White,   height - 1, 0);
		set<Knight>(Color::White, height - 1, 1);
		set<Bishop>(Color::White, height - 1, 2);
		set<Queen>(Color::White,  height - 1, 3);
		set<King>(Color::White,   height - 1, 4);
		set<Bishop>(Color::White, height - 1, 5);
		set<Knight>(Color::White, height - 1, 6);
		set<Rook>(Color::White,   height - 1, 7);

		for (int column = 0; column < width; ++column) {
			set<Pawn>(Color::Black, 1, column);
			set<Pawn>(Color::White, height - 2, column);
		}
	}

	void Board::loadFEN(const std::string &fen) {
		const auto rows = Algiz::split(fen, "/", false);
		for (int row = 0, max = rows.size(); row < max; ++row) {
			const auto row_string = rows[row];
			int column = 0;
			for (char ch: row_string) {
				if (Algiz::isNumeric(ch)) {
					column += ch - '0';
				} else {
					switch (ch) {
						case 'B': set<Bishop>(Color::White, row, column); break;
						case 'b': set<Bishop>(Color::Black, row, column); break;
						case 'K': set<King>  (Color::White, row, column); break;
						case 'k': set<King>  (Color::Black, row, column); break;
						case 'N': set<Knight>(Color::White, row, column); break;
						case 'n': set<Knight>(Color::Black, row, column); break;
						case 'P': set<Pawn>  (Color::White, row, column); break;
						case 'p': set<Pawn>  (Color::Black, row, column); break;
						case 'Q': set<Queen> (Color::White, row, column); break;
						case 'q': set<Queen> (Color::Black, row, column); break;
						case 'R': set<Rook>  (Color::White, row, column); break;
						case 'r': set<Rook>  (Color::Black, row, column); break;
						default: throw std::runtime_error("Unrecognized character in FEN: " + std::string(1, ch));
					}
					++column;
				}
			}
		}
	}

	void Board::erase(std::shared_ptr<Piece> piece) {
		if (!piece)
			return;
		pieces[piece->square.row][piece->square.column].reset();
	}

	void Board::erase(int row, int column) {
		erase(at(row, column));
	}

	bool Board::move(std::shared_ptr<Piece> piece, int new_row, int new_column) {
		if (!piece)
			return false;
		pieces[new_row][new_column] = piece;
		pieces[piece->square.row][piece->square.column] = nullptr;
		piece->square.row = new_row;
		piece->square.column = new_column;
		return true;
	}

	bool Board::move(std::shared_ptr<Piece> piece, Square square) {
		return move(piece, square.row, square.column);
	}

	std::list<std::shared_ptr<Piece>> Board::getPieces(Color color) const {
		std::list<std::shared_ptr<Piece>> out;
		for (int row = 0; row < height; ++row)
			for (int column = 0; column < width; ++row) {
				std::shared_ptr<Piece> piece = at(row, column);
				if (piece && piece->color == color)
					out.push_back(piece);
			}
		return out;
	}

	std::string Board::toFEN(Color color) const {
		std::stringstream out;
		for (int row = 0; row < height; ++row) {
			if (0 < row)
				out << "/";
			for (int column = 0; column < width; ++column) {
				std::shared_ptr<Piece> piece = at(row, column);
				if (piece)
					out << piece->toFEN();
				else {
					int i, blanks = 1;
					for (i = 1; column + i < width && !at(row, column + i); ++i)
						++blanks;
					column += i - 1;
					out << blanks;
				}
			}
		}

		out << " " << (color == Color::White? "w" : "b");
		out << " -"; // TODO: change when (if) castling is ever implemented.
		out << " -"; // TODO: change when (if) en passant is ever implemented.
		out << " 0 1";
		return out.str();
	}

	std::string Board::toString(std::shared_ptr<Piece> show_moves) const {
		const std::list<Square> moves = show_moves? show_moves->canMoveTo() : std::list<Square>();

		auto bg = [&](int row, int column) {
			for (const Square &square: moves)
				if (square.row == row && square.column == column)
					return "\e[43m";
			return row % 2 == column % 2? "\e[47m" : "\e[100m";
		};

		static auto pieceColor = [](std::shared_ptr<Piece> piece) {
			return piece->color == Color::White? "\e[37m" : "\e[30m";
		};

		std::stringstream out;

		for (int row = 0; row < height; ++row) {
			for (int column = 0; column < width; ++column)
				out << bg(row, column) << row << column << "   \e[0m";
			out << "\n";
			for (int column = 0; column < width; ++column) {
				std::shared_ptr<Piece> piece = at(row, column);
				out << bg(row, column) << "  ";
				if (piece)
					out << pieceColor(piece) << piece->toString(Color::Black);
				else
					out << " ";
				out << "  \e[0m";
			}
			out << "\n";
			for (int column = 0; column < width; ++column)
				out << bg(row, column) << "     \e[0m";
			out << "\n";
		}

		return out.str();
	}

	Board::operator std::string() const {
		return toString(nullptr);
	}
}

std::ostream & operator<<(std::ostream &os, const ProbChess::Board &board) {
	return os << std::string(board);
}
