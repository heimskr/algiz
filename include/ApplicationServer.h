#pragma once

namespace Algiz {
	class ApplicationServer {
		public:
			ApplicationServer() = delete;
			ApplicationServer(const ApplicationServer &) = delete;
			ApplicationServer(ApplicationServer &&) = delete;

			virtual ~ApplicationServer() = default;

			ApplicationServer & operator=(const ApplicationServer &) = delete;
			ApplicationServer & operator=(ApplicationServer &&) = delete;

			virtual void run() = 0;
			virtual void stop() = 0;
	};
}
