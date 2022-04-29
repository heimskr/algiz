#pragma once

#include <cstddef>
#include <string>

namespace Algiz {
	struct GenericClient {
		int id = -1;
		bool lineMode = true;
		size_t maxLineSize = -1;

		GenericClient() = delete;
		GenericClient(const GenericClient &) = delete;
		GenericClient(GenericClient &&) = delete;
		GenericClient(int id_, bool line_mode, size_t max_line_size = -1):
			id(id_), lineMode(line_mode), maxLineSize(max_line_size) {}

		virtual ~GenericClient() = default;

		GenericClient & operator=(const GenericClient &) = delete;
		GenericClient & operator=(GenericClient &&) = delete;

		virtual void handleInput(std::string_view) = 0;
		virtual void onMaxLineSizeExceeded() {}
	};
}
