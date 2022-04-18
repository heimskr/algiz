#include <iostream>
#include <string>
#include <vector>

#include "net/Server.h"
#include "Core.h"
#include "Options.h"

int main(int argc, char **argv) {
	Algiz::Options options = Algiz::Options::parse(argc, argv);
	std::cout << options.ip << ", " << options.port << ", " << options.root << '\n';
	auto server = std::unique_ptr<Algiz::Server>(Algiz::run(options));
	server->run();
}
