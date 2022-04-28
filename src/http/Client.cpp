#include <bit>

#include "Log.h"
#include "error/ParseError.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	void Client::send(const std::string &message) {
		server.server->send(id, message);
	}

#define CHECKSIZE(n) do { if (message_size < (n)) { server.send400(*this); removeSelf(); return; } } while (false)

	void Client::handleInput(const std::string &message) {
		if (isWebSocket) {
			std::cerr << std::string(16, '=') << ' ' << message.size() << ' ' << std::string(16, '=') << '\n';
			for (unsigned char ch: message)
				std::cerr << hex(unsigned(ch)) << ' ' << escapeANSI(std::string(1, ch)) << '\n';
			std::cerr << std::string(34 + std::to_string(message.size()).size(), '=') << '\n';

			const size_t message_size = message.size();
			CHECKSIZE(2);

			const uint8_t byte1 = message[0];
			const bool fin = ((byte1 >> 7) & 1) == 1;
			const uint8_t opcode = byte1 & 0xf;
			const uint8_t byte2 = message[1];
			const bool use_mask = ((byte2 >> 7) & 1) == 1;
			size_t payload_length = byte2 & 0x7f;

			if (payload_length == 126) {
				CHECKSIZE(4);
				payload_length = (size_t(uint8_t(message[2])) << 8) | uint8_t(message[3]);
			} else if (payload_length == 127) {
				CHECKSIZE(10);
				std::memcpy(&payload_length, &message[2], sizeof(payload_length));
				if (std::endian::native == std::endian::little)
					payload_length = __builtin_bswap64(payload_length);
			}

			if (!use_mask) {
				server.send400(*this);
				removeSelf();
				return;
			}
		} else {
			const auto result = request.handleLine(message);
			if (result == Request::HandleResult::DisableLineMode)
				lineMode = false;
			else if (result == Request::HandleResult::Done)
				handleRequest();
		}
	}

	void Client::handleRequest() {
		switch (request.method) {
			case Request::Method::GET:
				server.handleGet(*this, request);
				break;
			default:
				throw ParseError("Invalid method: " + std::to_string(int(request.method)));
		}
	}

	void Client::onMaxLineSizeExceeded() {
		send(Response(413, "Payload too large"));
	}

	void Client::removeSelf() {
		server.server->removeClient(id);
	}

	std::unordered_set<std::string> Client::supportedMethods {"GET"};
}
