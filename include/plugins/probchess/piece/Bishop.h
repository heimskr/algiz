#pragma once

#include "plugins/probchess/piece/Piece.h"

namespace ProbChess {
	class Bishop: public Piece {
		public:
			using Piece::Piece;
			virtual std::list<Square> canMoveTo() const override;
			virtual Piece * clone(Board *new_board) const override { return new Bishop(new_board, color, square); }
			virtual Type getType() const override { return Type::Bishop; }
			virtual int typeValue() const override { return 3; }
			virtual std::string name() const override { return "bishop"; }
			virtual std::string toString(Color color = Color::Black) const override { return color == Color::White? "♗" : "♝"; }
			virtual std::string toFEN() const override { return color == Color::White? "B" : "b"; }
	};
}
