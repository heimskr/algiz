#pragma once

#include <cstdint>
#include <string>

namespace Algiz {
	struct Options {
		enum class AddressFamily {None, IPv4, IPv6};

		AddressFamily addressFamily = AddressFamily::None;
		std::string ip;
		uint16_t port = 0;
		std::string root;

		static Options parse(int argc, char * const *argv);
	};
}
