#pragma once

#include <filesystem>

#include "net/Server.h"
#include "Options.h"

namespace Algiz::HTTP {
	class Client;

	class Server: public Algiz::Server {
		protected:
			std::filesystem::path webRoot;

			void addClient(int) override;

		public:
			Server(const Options &);

			void handleGet(HTTP::Client &, const std::string &path);
	};
}
