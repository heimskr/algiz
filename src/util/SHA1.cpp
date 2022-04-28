#include "util/SHA1.h"

namespace Algiz {
	std::string sha1(std::string_view in) {
		unsigned char hash[SHA_DIGEST_LENGTH];
		SHA1(reinterpret_cast<const unsigned char *>(in.begin()), in.size(), hash);
		return {reinterpret_cast<char *>(hash), sizeof(hash)};
	}
}
