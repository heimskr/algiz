#include "util/Usage.h"

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/task_info.h>
#else
#include <fstream>
#endif

namespace Algiz {
	bool getMemoryUsage(size_t &virtual_memory, size_t &resident_memory) {
#ifdef __APPLE__
		task_basic_info t_info;
		mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
		if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, task_info_t(&t_info), &t_info_count))
			return false;
		virtual_memory = t_info.virtual_size / 1024;
		resident_memory = t_info.resident_size / 1024;
		return true;
#else
		// https://stackoverflow.com/a/671389/227663
		try {
			std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);
			std::string skip;
			stat_stream >> skip >> skip >> skip >> skip >> skip >> skip
			            >> skip >> skip >> skip >> skip >> skip >> skip
			            >> skip >> skip >> skip >> skip >> skip >> skip
			            >> skip >> skip >> skip >> skip >> skip >> resident_memory;
		} catch (const std::exception &) {
			return false;
		}

		return true;
#endif
	}
}
