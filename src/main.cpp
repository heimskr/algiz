#include <algorithm>
#include <csignal>
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

std::vector<std::unique_ptr<Algiz::ApplicationServer>> global_servers;

int main(int argc, char **argv) {
	signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, +[](int) {
		for (auto &server: global_servers)
			server->stop();
	});

	nlohmann::json options = nlohmann::json::parse(Algiz::readFile(argc == 1? "algiz.json" : argv[1]));

	global_servers = map(Algiz::run(options), [](auto *server) {
		return std::unique_ptr<Algiz::ApplicationServer>(server);
	});

	std::vector<std::thread> threads;
	threads.reserve(global_servers.size());

	for (auto &server: global_servers)
		threads.emplace_back([server = server.get()] {
			server->run();
		});

	for (auto &thread: threads)
		thread.join();

	global_servers.clear();
}
