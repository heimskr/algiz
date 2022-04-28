#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>

#include "ApplicationServer.h"
#include "http/Request.h"
#include "net/Server.h"
#include "nlohmann/json.hpp"
#include "plugins/PluginHost.h"
#include "util/StringVector.h"
#include "util/WeakCompare.h"

namespace Algiz::HTTP {
	class Client;

	class Server: public ApplicationServer, public Plugins::PluginHost {
		public:
			struct HandlerArgs {
				Server &server;
				Client &client;
				Request request;
				const StringVector parts;

				explicit HandlerArgs(Server &server_, Client &client_, Request &&request_,
				                     StringVector &&parts_):
					server(server_), client(client_), request(std::move(request_)), parts(std::move(parts_)) {}
			};

			struct WebSocketArgs: HandlerArgs {
				/** The list of protocols requested by the client. The server should pick exactly one of these.
				 *  May be empty. */
				const StringVector protocols;

				/** The protocol accepted by the server. Sent to the client with the Sec-WebSocket-Protocol header.
				 *  Expected to be changed by connection handlers. */
				std::string acceptedProtocol;

				explicit WebSocketArgs(Server &server_, Client &client_, Request &&request_, StringVector &&parts_,
				                       StringVector &&protocols_):
					HandlerArgs(server_, client_, std::move(request_), std::move(parts_)),
					protocols(std::move(protocols_)) {}
			};

			using MessageHandler = PreFn<std::string_view>;
			using MessageHandlerPtr = std::shared_ptr<MessageHandler>;
			using WeakMessageHandlerPtr = std::weak_ptr<MessageHandler>;

		private:
			std::map<int, WeakSet<MessageHandler>> webSocketMessageHandlers;

			[[nodiscard]] std::filesystem::path getWebRoot(const std::string &) const;
			[[nodiscard]] static bool validatePath(const std::string_view &);
			[[nodiscard]] std::vector<std::string> getParts(const std::string_view &) const;

		public:
			std::shared_ptr<Algiz::Server> server;
			nlohmann::json options;
			std::filesystem::path webRoot;
			std::list<PrePtr<HandlerArgs>> handlers;
			WeakSet<PluginHost::PreFn<WebSocketArgs>> webSocketConnectionHandlers;

			Server() = delete;
			Server(const Server &) = delete;
			Server(Server &&) = delete;
			Server(const std::shared_ptr<Algiz::Server> &, const nlohmann::json &options_);

			~Server() override;

			Server & operator=(const Server &) = delete;
			Server & operator=(Server &&) = delete;

			void run() override;
			void stop() override;
			void handleGet(HTTP::Client &, const Request &);
			void send400(HTTP::Client &);
			void send403(HTTP::Client &);
			void send500(HTTP::Client &);
			void cleanWebSocketHandlers();
			void registerWebSocketMessageHandler(const HTTP::Client &, WeakMessageHandlerPtr);
	};
}
