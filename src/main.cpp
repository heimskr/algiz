#include <algorithm>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "http/Server.h"
#include "net/Server.h"
#include "util/FS.h"
#include "util/Util.h"
#include "ApplicationServer.h"
#include "Core.h"
#include "Options.h"

int main(int argc, char **argv) {
	// Algiz::Options options = Algiz::Options::parse(argc, argv);
	nlohmann::json options = nlohmann::json::parse(Algiz::readFile(argc == 1? "algiz.json" : argv[1]));

	auto servers = map<std::shared_ptr<Algiz::ApplicationServer>>(Algiz::run(options), [](auto *server) {
		return std::shared_ptr<Algiz::ApplicationServer>(server);
	});

	std::vector<std::thread> threads;
	threads.reserve(servers.size());

	for (auto &server: servers) {
		threads.emplace_back([server] {
			server->run();
		});
	}

	for (auto &thread: threads) {
		thread.join();
	}
}
