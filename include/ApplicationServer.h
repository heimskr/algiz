#pragma once

namespace Algiz {
	class ApplicationServer {
		public:
			virtual ~ApplicationServer() {}
			virtual void run() = 0;
			virtual void stop() = 0;
	};
}
