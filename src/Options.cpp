#include <getopt.h>
#include <iostream>
#include <unistd.h>

#include "Options.h"
#include "Util.h"

namespace Algiz {
	Options Options::parse(int argc, char * const *argv) {
		Options out {{}, 80, "./www"};

		static option long_options[] = {
			{"ip",   required_argument, nullptr, 'i'},
			{"port", required_argument, nullptr, 'p'},
			{"root", required_argument, nullptr, 'r'},
			{nullptr, 0, nullptr, 0}
		};

		for (;;) {
			int option_index;
			int opt = getopt_long(argc, argv, "i:p:r:", long_options, &option_index);
			if (opt == -1)
				break;

			switch (opt) {
				case 'i':
					out.ip = optarg;
					break;
				
				case 'p': {
					unsigned long port = parseUlong(optarg, 10);
					if (65535 < port)
						throw std::invalid_argument("Port out of range: " + std::to_string(port));
					out.port = static_cast<unsigned short>(port);
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
