#pragma once

#include <any>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>

#include "http/Request.h"
#include "net/GenericClient.h"
#include "util/StringVector.h"

namespace Algiz::HTTP {
	class Server;

	enum class WebSocketMessageType {Invalid, Binary, Text};

	class Client: public GenericClient {
		private:
			HTTP::Server &server;
			bool awaitingWebSocketHeader = true;
			bool lastFin = false;
			uint32_t webSocketMask = 0;
			uint32_t maskOffset = 0;
			uint64_t remainingBytesInPacket = 0;
			std::string packet;
			std::string leftoverMessage;

			void handleRequest();

		public:
			Request request;
			std::map<std::string, std::any> session;
			bool isWebSocket = false;
			StringVector webSocketPath;
			size_t maxWebSocketPacketLength = 1 << 24;

			Client() = delete;
			Client(HTTP::Server &server_, int id_): GenericClient(id_, true, 8192), server(server_) {}
			Client(const Client &) = delete;
			Client(Client &&) = delete;

			~Client() = default;

			Client & operator=(const Client &) = delete;
			Client & operator=(Client &&) = delete;

			void send(std::string_view);
			void send(const std::string &);
			void handleInput(std::string_view) override;
			void sendWebSocket(std::string_view, bool is_binary = false, uint8_t opcode_override = 255);
			void closeWebSocket();
			void onMaxLineSizeExceeded() override;
			void removeSelf();

			static std::unordered_set<std::string> supportedMethods;
	};
}
