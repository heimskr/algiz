#pragma once

#include <functional>
#include <map>
#include <string>
#include <unordered_set>

#include "net/GenericClient.h"

namespace Algiz::HTTP {
	class Client: public GenericClient {
		private:
			void handleRequest();

		public:
			enum class Mode {Method, Headers, Content};

			Mode mode = Mode::Method;
			std::map<std::string, std::string> headers;
			std::string content;
			std::string method;
			std::string path;

			std::function<void(const std::string &)> send = [](const std::string &) {};

			using GenericClient::GenericClient;

			void handleInput(const std::string &) override;

			static std::unordered_set<std::string> supportedMethods;
	};
}
