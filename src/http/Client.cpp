#include <bit>

#include "Log.h"
#include "error/ParseError.h"
#include "error/UnsupportedMethod.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "util/Util.h"

namespace Algiz::HTTP {
	void Client::send(std::string_view message) {
		server.server->send(id, message);
	}

	void Client::send(const std::string &message) {
		server.server->send(id, message);
	}

	void Client::close() {
		if (!keepAlive)
			server.server->close(id);
	}

#define CHECKSIZE(n) do { if (message_size < (n)) { if (!has_leftover) leftoverMessage = message; return; } } \
	while (false)

	void Client::handleInput(std::string_view message_in) {
		if (isWebSocket) {
			const bool has_leftover = !leftoverMessage.empty();
			if (has_leftover)
				leftoverMessage += message_in;
			std::string_view message = has_leftover? leftoverMessage : message_in;
			const size_t message_size = message.size();

			if (awaitingWebSocketHeader) {
				CHECKSIZE(2);

				const uint8_t byte1 = message[0];
				const bool fin = ((byte1 >> 7) & 1) == 1;
				lastFin = fin;
				const uint8_t opcode = byte1 & 0xf;
				const uint8_t byte2 = message[1];
				const bool use_mask = ((byte2 >> 7) & 1) == 1;
				uint64_t payload_length = byte2 & 0x7f;

				size_t mask_index = 2;

				if (payload_length == 126) {
					CHECKSIZE(4);
					payload_length = (uint64_t(uint8_t(message[2])) << 8) | uint8_t(message[3]);
					mask_index += 2;
				} else if (payload_length == 127) {
					CHECKSIZE(10);
					std::memcpy(&payload_length, &message[2], sizeof(payload_length));
					if (std::endian::native == std::endian::little)
						payload_length = __builtin_bswap64(payload_length);
					mask_index += 8;
				}

				if (!use_mask || maxWebSocketPacketLength < payload_length) {
					server.send400(*this);
					removeSelf();
					return;
				}

				CHECKSIZE(mask_index + 4);
				std::memcpy(&webSocketMask, &message[mask_index], sizeof(webSocketMask));
				if (std::endian::native == std::endian::little)
					webSocketMask = __builtin_bswap32(webSocketMask);

				const uint64_t payload_length_here = std::min(payload_length, uint64_t(message_size - mask_index - 4));
				remainingBytesInPacket = payload_length - payload_length_here;
				packet.reserve(payload_length);
				maskOffset = 0;

				// There must be a more efficient way than doing this byte by byte.
				for (uint64_t i = 0; i < payload_length_here; ++i) {
					packet += char(message[i + mask_index + 4] ^ uint8_t(webSocketMask >> (8 * (3 - maskOffset))));
					maskOffset = (maskOffset + 1) % 4;
				}

				if (opcode == 9) {
					if (payload_length <= 125)
						sendWebSocket(packet, true, 10);
					else // Pings cannot have a payload greater than 125 bytes in length.
						closeWebSocket();
				} else {
					const uint64_t end = payload_length_here + mask_index + 4;
					if (end < message_size) {
						awaitingWebSocketHeader = true;
						leftoverMessage = message.substr(end);
						if (fin) {
							remainingBytesInPacket = 0;
							server.handleWebSocketMessage(*this, packet);
						} else
							closeWebSocket();
						packet.clear();
					} else {
						if (end == message_size && remainingBytesInPacket == 0) {
							awaitingWebSocketHeader = true;
							if (fin)
								server.handleWebSocketMessage(*this, packet);
							else
								closeWebSocket();
							packet.clear();
						} else
							awaitingWebSocketHeader = false;
						leftoverMessage.clear();
					}
				}
			} else {
				const uint64_t payload_length_here = std::min(remainingBytesInPacket, uint64_t(message_size));

				for (uint64_t i = 0; i < payload_length_here; ++i) {
					packet += char(message[i] ^ uint8_t(webSocketMask >> (8 * (3 - maskOffset))));
					maskOffset = (maskOffset + 1) % 4;
				}

				if (remainingBytesInPacket < message_size) {
					leftoverMessage = message.substr(payload_length_here);
					remainingBytesInPacket = 0;
				} else
					remainingBytesInPacket -= payload_length_here;

				if (remainingBytesInPacket == 0) {
					awaitingWebSocketHeader = true;
					if (lastFin) {
						server.handleWebSocketMessage(*this, packet);
						packet.clear();
					}
				}
			}
		} else {
			try {
				const auto result = request.handleLine(message_in);
				if (result == Request::HandleResult::DisableLineMode)
					lineMode = false;
				else if (result == Request::HandleResult::Done)
					handleRequest();
			} catch (const UnsupportedMethod &) {
				server.send400(*this);
				removeSelf();
			}
		}
	}

	void Client::sendWebSocket(std::string_view message, bool is_binary, uint8_t opcode_override) {
		const size_t message_size = message.size();
		const uint8_t opcode = opcode_override != 255? opcode_override : (is_binary? 2 : 1);

		std::string header;
		header.reserve(10);

		header += uint8_t(1 << 7) | opcode;

		uint8_t byte2 = 0 << 7;

		if (message_size <= 125) {
			byte2 |= message_size;
			header += byte2;
		} else if (message_size < 65536) {
			byte2 |= 126;
			header += byte2;
			header += uint8_t((message_size >> 8) & 0xff);
			header += uint8_t((message_size >> 0) & 0xff);
		} else {
			byte2 |= 127;
			header += byte2;
			header += uint8_t((message_size >> 56) & 0xff);
			header += uint8_t((message_size >> 48) & 0xff);
			header += uint8_t((message_size >> 40) & 0xff);
			header += uint8_t((message_size >> 32) & 0xff);
			header += uint8_t((message_size >> 24) & 0xff);
			header += uint8_t((message_size >> 16) & 0xff);
			header += uint8_t((message_size >>  8) & 0xff);
			header += uint8_t((message_size >>  0) & 0xff);
		}

		send(header);
		send(message);
	}

	void Client::closeWebSocket() {
		sendWebSocket("Error", false, 8);
		server.closeWebSocket(*this);
	}

	void Client::handleRequest() {
		switch (request.method) {
			case Request::Method::GET:
				server.handleGET(*this, request);
				break;
			case Request::Method::POST:
				server.handlePOST(*this, request);
				break;
			default:
				throw ParseError("Invalid method: " + std::to_string(int(request.method)));
		}
	}

	void Client::onMaxLineSizeExceeded() {
		send(Response(413, "Payload too large"));
	}

	void Client::removeSelf() {
		server.server->close(id);
	}

	std::string Client::getID() const {
		return server.server->id + ":" + std::to_string(id);
	}

	std::unordered_set<std::string> Client::supportedMethods {"GET"};
}
