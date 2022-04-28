#include <vector>

#include "util/Base64.h"

namespace Algiz {
	static constexpr const char *CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	// https://stackoverflow.com/a/34571089/227663
	std::string base64Encode(std::string_view in) {
		std::string out;

		int val = 0;
		int valb = -6;

		for (unsigned char c: in) {
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
		std::vector<int> t(256, -1);

		for (int i = 0; i < 64; ++i)
			t[CHARS[i]] = i;

		int val = 0;
		int valb = -8;

		for (unsigned char c: in) {
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