#include <getopt.h>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

#include "Options.h"
#include "util/Util.h"

namespace Algiz {
	Options Options::parse(int argc, char * const *argv) {
		Options out {-1, {}, 80, "./www"};

		static option long_options[] = {
			{"ip4",  required_argument, nullptr, '4'},
			{"ip6",  required_argument, nullptr, '6'},
			{"port", required_argument, nullptr, 'p'},
			{"root", required_argument, nullptr, 'r'},
			{nullptr, 0, nullptr, 0}
		};

		for (;;) {
			int option_index;
			int opt = getopt_long(argc, argv, "4:6:p:r:", long_options, &option_index);
			if (opt == -1)
				break;

			switch (opt) {
				case '4':
				case '6':
					if (out.addressFamily == -1) {
						out.ip = optarg;
						out.addressFamily = opt == '4'? AF_INET : AF_INET6;
					} else
						throw std::invalid_argument("IP already specified");
					break;
				
				case 'p': {
					unsigned long port = parseUlong(optarg, 10);
					if (65535 < port)
						throw std::invalid_argument("Port out of range: " + std::to_string(port));
					out.port = uint16_t(port);
					break;
				}

				case 'r':
					out.root = optarg;
					break;

				default:
					throw std::invalid_argument("Failed to parse options");
			}
		}

		return out;
	}
}
