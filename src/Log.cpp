#include "Log.h"
#include "util/Util.h"

namespace Algiz {
	Logger log;
	std::mutex logMutex;

	std::string Logger::getTimestamp() {
		return formatTime("%H:%M:%S");
	}
}
