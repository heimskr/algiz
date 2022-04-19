#include <getopt.h>
#include <filesystem>
#include <iostream>
#include <optional>
#include <unistd.h>
#include <sys/socket.h>

#include "Options.h"
#include "util/FS.h"
#include "util/Util.h"
#include "Log.h"

namespace Algiz {
	Options Options::parse(int argc, char * const *argv) {
		static option long_options[] = {
			{"config", required_argument, nullptr, 'c'},
			{"ip4",    required_argument, nullptr, '4'},
			{"ip6",    required_argument, nullptr, '6'},
			{"port",   required_argument, nullptr, 'p'},
			{nullptr, 0, nullptr, 0}
		};

		std::string configuration_file = "algiz.json";
		bool configuration_specified = false;

		std::optional<uint16_t> port;
		std::optional<std::string> root;
		std::optional<std::string> ip;
		std::optional<int> address_family;

		for (;;) {
			int option_index;
			int opt = getopt_long(argc, argv, "4:6:p:c:", long_options, &option_index);
			if (opt == -1)
				break;

			switch (opt) {
				case 'c':
					configuration_file = optarg;
					configuration_specified = true;
					break;

				case '4':
				case '6':
					if (!address_family) {
						ip = optarg;
						address_family = opt == '4'? AF_INET : AF_INET6;
					} else
						throw std::invalid_argument("IP already specified");
					break;
				
				case 'p': {
					unsigned long long_port = parseUlong(optarg, 10);
					if (65535 < long_port)
						throw std::invalid_argument("Port out of range: " + std::to_string(long_port));
					port = uint16_t(long_port);
					break;
				}

				default:
					throw std::invalid_argument("Failed to parse options");
			}
		}

		nlohmann::json json;

		if (!std::filesystem::exists(configuration_file)) {
			if (configuration_specified)
				throw std::runtime_error("Couldn't load configuration file " + configuration_file);
		} else {
			json = nlohmann::json::parse(readFile(configuration_file));

			if (!ip && json.contains("ip")) {
				ip = json.at("ip");
				address_family = ip->find(':') != std::string::npos? AF_INET6 : AF_INET;
			}

			if (!port && json.contains("port"))
				port = json.at("port");
		}

		if (!ip) {
			WARN("IP not specified; defaulting to ::");
			ip = "::";
			address_family = AF_INET6;
		}

		if (!port) {
			WARN("Port not specified; defaulting to 80");
			port = 80;
		}

		return {*address_family, *ip, *port, json};
	}
}
