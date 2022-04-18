#pragma once

#include "net/Server.h"

namespace Algiz::HTTP {
	class Server: public Algiz::Server {
		protected:
			void addClient(int) override;

		public:
			using Algiz::Server::Server;
	};
}
