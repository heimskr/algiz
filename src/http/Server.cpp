#include "http/Client.h"
#include "http/Server.h"

namespace Algiz::HTTP {
	void Server::addClient(int new_client) {
		auto http_client = std::make_unique<HTTP::Client>(*this, new_client);
		allClients.try_emplace(new_client, std::move(http_client));
	}
}
