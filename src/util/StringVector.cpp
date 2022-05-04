#include "util/StringVector.h"
#include "util/Util.h"

namespace Algiz {
	StringVector::StringVector(std::vector<std::string_view> &&strings) {
		for (auto view: strings)
			emplace_back(view);
	}

	std::string StringVector::join(const char *delimiter) const {
		return Algiz::join(*this, delimiter);
	}

	bool StringVector::operator==(std::initializer_list<const char *> others) const {
		if (others.size() != size())
			return false;
		
		const auto *iter = others.begin();
		
		for (const auto &string: *this) {
			if (*iter != nullptr && *iter != string)
				return false;
			++iter;
		}

		return true;
	}
}
