#include <iostream>
#include <string>
#include <vector>

#include "Options.h"

int main(int argc, char **argv) {
	Algiz::Options options = Algiz::Options::parse(argc, argv);
	std::cout << options.ip << ", " << options.port << ", " << options.root << '\n';
}
