#include <array>
#include <cstdint>

#include "util/Base64.h"

namespace Algiz {
	static constexpr const char *CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	static std::array<int, 64> t;
	static bool b = [] {
		t.fill(-1);
		for (uint8_t i = 0; i < 64; ++i)
			t[CHARS[i]] = i;
		return false;
	}();

	// https://stackoverflow.com/a/34571089/227663
	std::string base64Encode(std::string_view in) {
		std::string out;

		int val = 0;
		int valb = -6;

		for (uint8_t c: in) {
			val = (val << 8) + c;
			valb += 8;
			while (0 <= valb) {
				out += CHARS[(val >> valb) & 0x3f];
				valb -= 6;
			}
		}

		if (-6 < valb)
			out += CHARS[((val << 8) >> (valb + 8)) & 0x3f];

		while (out.size() % 4 != 0)
			out += '=';

		return out;
	}

	// https://stackoverflow.com/a/34571089/227663
	std::string base64Decode(std::string_view in) {
		std::string out;

		int val = 0;
		int valb = -8;

		for (uint8_t c: in) {
			if (t[c] == -1)
				break;
			val = (val << 6) + t[c];
			valb += 6;
			if (0 <= valb) {
				out += char((val >> valb) & 0xff);
				valb -= 8;
			}
		}

		return out;
	}
}