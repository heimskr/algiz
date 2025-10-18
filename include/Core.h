#pragma once

#include <memory>
#include <vector>

#include "nlohmann/json.hpp"

namespace Algiz {
	class ApplicationServer;
	class SSLServer;

	constexpr size_t DEFAULT_THREAD_COUNT = 8;

	class Core {
		public:
			Core() = default;

			void run(nlohmann::json &);

			const auto & getServers() const { return servers; }

			std::shared_ptr<SSLServer> getSSLServer() const;

		private:
			std::vector<ApplicationServer *> servers;
	};

}
