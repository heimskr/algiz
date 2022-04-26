#pragma once

#include <string>

namespace Algiz {
	struct GenericClient {
		int id = -1;
		bool lineMode = true;

		GenericClient() = delete;
		GenericClient(const GenericClient &) = delete;
		GenericClient(GenericClient &&) = delete;
		GenericClient(int id_, bool line_mode): id(id_), lineMode(line_mode) {}

		virtual ~GenericClient() = default;

		GenericClient & operator=(const GenericClient &) = delete;
		GenericClient & operator=(GenericClient &&) = delete;

		virtual void handleInput(const std::string &message) = 0;
	};
}
