#pragma once

#include <map>
#include <string>

namespace Algiz {
	std::string getMIME(const std::string &extension);

	extern std::map<std::string, std::string> mimeTypes;
}