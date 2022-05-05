#pragma once

#include <array>
#include <list>
#include <memory>

#include "Move.h"
#include "piece/Piece.h"

// Assumption: black starts at the top.

namespace ProbChess {
	class Board {
		public:
			std::array<std::array<std::shared_ptr<Piece>, 8>, 8> pieces;
			int width = 8, height = 8;

			Board() {}
			Board(const Board &);
			Board(Board &&) = delete;

			Board & operator=(const Board &);
			Board & operator=(Board &&) = delete;

			std::shared_ptr<Piece> at(int row, int column) const;
			std::shared_ptr<Piece> at(Square) const;
			/** Returns true if a square could be attacked by the given color, supposing it contains a piece. */
			bool isAttacked(Color, const Square &);
			std::shared_ptr<Piece> findKing(Color) const;
			/** Returns true if the given color's king is in check. */
			bool isInCheck(Color);
			bool isCheckmated(Color);
			std::list<Move> allMoves() const;
			std::list<Move> allMoves(Color) const;
			void placePieces();
			void loadFEN(const std::string &);
			void erase(std::shared_ptr<Piece>);
			void erase(int row, int column);
			bool move(std::shared_ptr<Piece>, int new_row, int new_column);
			bool move(std::shared_ptr<Piece>, Square);
			std::list<std::shared_ptr<Piece>> getPieces(Color) const;
			std::string toFEN(Color) const;
			std::string toString(std::shared_ptr<Piece>) const;
			operator std::string() const;

			template <typename P>
			std::shared_ptr<Piece> set(Color color, int row_, int column_) {
				return pieces[row_][column_] = std::make_shared<P>(this, color, row_, column_);
			}
	};
}

std::ostream & operator<<(std::ostream &, const ProbChess::Board &);
