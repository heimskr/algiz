#ifndef CHESS_NOKINGERROR_H_
#define CHESS_NOKINGERROR_H_

#include <stdexcept>

struct NoKingError: public std::runtime_error {
	using std::runtime_error::runtime_error;
};

#endif
