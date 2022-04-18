#pragma once

#include <map>
#include <string>

#include "net/GenericClient.h"

namespace Algiz::HTTP {
	class Client: public GenericClient {
		public:
			enum class Mode {Method, Headers, Content};

			Mode mode = Mode::Method;
			std::map<std::string, std::string> headers;
			std::string content;
		
			using GenericClient::GenericClient;

			void handleInput(const std::string &) override;
	};
}
