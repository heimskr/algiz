#include "util/StringVector.h"

namespace Algiz {
	StringVector::StringVector(std::vector<std::string_view> &&strings) {
		for (auto view: strings)
			emplace_back(view);
	}

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
