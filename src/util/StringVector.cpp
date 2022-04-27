#include "util/StringVector.h"

namespace Algiz {
	bool StringVector::operator==(std::initializer_list<const char *> others) const {
		if (others.size() != size())
			return false;
		
		auto iter = others.begin();
		
		for (const auto &string: *this) {
			if (*iter != nullptr && *iter != string)
				return false;
			++iter;
		}

		return true;
	}
}
