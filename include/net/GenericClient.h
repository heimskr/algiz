#pragma once

#include <cstddef>
#include <string>

namespace Algiz {
	struct GenericClient {
		int id = -1;
		std::string ip;
		bool lineMode = true;
		size_t maxLineSize = -1;
		/** If nonzero, don't read more than this many bytes at a time. The amount read will be subtracted from this. */
		size_t maxRead = 0;

		GenericClient() = delete;
		GenericClient(const GenericClient &) = delete;
		GenericClient(GenericClient &&) = delete;
		GenericClient(int id_, std::string_view ip_, bool line_mode, size_t max_line_size = -1):
			id(id_), ip(ip_), lineMode(line_mode), maxLineSize(max_line_size) {}

		virtual ~GenericClient() = default;

		GenericClient & operator=(const GenericClient &) = delete;
		GenericClient & operator=(GenericClient &&) = delete;

		virtual void handleInput(std::string_view) = 0;
		virtual void onMaxLineSizeExceeded() {}
		virtual std::string describe();
	};
}
