#pragma once

namespace Algiz {
	class ApplicationServer {
		public:
			virtual ~ApplicationServer() {}
			virtual void run() = 0;
	};
}
