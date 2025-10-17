#include <algorithm>
#include <csignal>
#include <execinfo.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <event2/thread.h>

#include "ApplicationServer.h"
#include "Core.h"
#include "http/Server.h"
#include "net/Server.h"
#include "util/FS.h"
#include "util/GeoIP.h"
#include "util/Util.h"

std::vector<std::unique_ptr<Algiz::ApplicationServer>> global_servers;

int main(int argc, char **argv) {
	try {
		evthread_use_pthreads();

		if (2 < argc && strcmp(argv[1], "--home") == 0) {
			std::filesystem::current_path(argv[2]);
		}

		if (1 < argc && strcmp(argv[1], "dbg") == 0) {
			std::set_terminate(+[] {
				void *trace_elems[64];
				int trace_elem_count = backtrace(trace_elems, sizeof(trace_elems) / sizeof(trace_elems[0]));
				char **stack_syms = backtrace_symbols(trace_elems, trace_elem_count);
				std::cerr << '\n';
				for (int i = 0; i < trace_elem_count; ++i) {
					std::cerr << stack_syms[i] << '\n';
				}
				free(stack_syms);
				exit(1);
			});
		}

		{
			struct sigaction sa;
			sa.sa_handler = SIG_IGN;
			sa.sa_flags = 0;
			if (sigemptyset(&sa.sa_mask) == -1 || sigaction(SIGPIPE, &sa, 0) == -1) {
				throw std::runtime_error("Couldn't register SIGPIPE handler");
			}
		}

		if (signal(SIGINT, +[](int) { for (auto &server: global_servers) server->stop(); }) == SIG_ERR) {
			throw std::runtime_error("Couldn't register SIGINT handler");
		}

		nlohmann::json options = nlohmann::json::parse(Algiz::readFile(argc <= 1 || std::string_view(argv[1]) == "dbg"? "algiz.json" : argv[1]));

		if (auto iter = options.find("geoip"); iter != options.end()) {
			INFO("Reading MMDB data from " << *iter << '.');
			Algiz::GeoIP::get(*iter);
		}

		global_servers = map(Algiz::run(options), [](auto *server) {
			return std::unique_ptr<Algiz::ApplicationServer>(server);
		});

		std::vector<std::thread> threads;
		threads.reserve(global_servers.size());

		for (auto &server: global_servers) {
			threads.emplace_back([server = server.get()] {
				server->run();
			});
		}

		for (auto &thread: threads) {
			thread.join();
		}

		global_servers.clear();
		return 0;
	} catch (const std::exception &err) {
		ERROR(err.what());
		return 1;
	}
}
