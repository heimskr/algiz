#include <chrono>
#include <mutex>

#include "Log.h"

namespace Algiz {
	Logger log;
	std::mutex localtimeMutex;

	std::string Logger::getTimestamp() {
		char string[9];
		std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		localtimeMutex.lock();
		const tm *now_tm = localtime(&now);
		localtimeMutex.unlock();
		strftime(string, sizeof(string), "%H:%M:%S", now_tm);
		return string;
	}
}
