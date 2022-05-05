#ifndef CHESS_CHESSERROR_H_
#define CHESS_CHESSERROR_H_

#include <stdexcept>

struct ChessError: std::runtime_error {
	using std::runtime_error::runtime_error;
};

#endif
