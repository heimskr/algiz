#include "net/Server.h"
#include "Core.h"
#include "Log.h"

namespace Algiz {
	Server * run(const Options &options) {
		auto *server = new Server(options.addressFamily, options.ip, options.port, true, 1024);
		server->messageHandler = [](int client, const std::string &message) {
			log << "Message from client " << client << " (" << message.size() << "byte(s)): [" << message << "]\n";
		};

		return server;
	}
}
