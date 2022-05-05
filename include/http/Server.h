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

			struct WebSocketConnectionArgs: HandlerArgs {
				/** The list of protocols requested by the client. The server should pick exactly one of these.
				 *  May be empty. */
				const StringVector protocols;

				/** The protocol accepted by the server. Sent to the client with the Sec-WebSocket-Protocol header.
				 *  Expected to be changed by connection handlers. */
				std::string acceptedProtocol;

				explicit WebSocketConnectionArgs(Server &server_, Client &client_, Request &&request_, StringVector &&parts_,
				                       StringVector &&protocols_):
					HandlerArgs(server_, client_, std::move(request_), std::move(parts_)),
					protocols(std::move(protocols_)) {}
			};

			struct WebSocketMessageArgs {
				Server &server;
				Client &client;
				std::string_view message;

				explicit WebSocketMessageArgs(Server &server_, Client &client_, std::string_view message_):
					server(server_), client(client_), message(message_) {}
			};

			using MessageHandler = PreFn<WebSocketMessageArgs &>;
			using MessageHandlerPtr = std::shared_ptr<MessageHandler>;
			using WeakMessageHandlerPtr = std::weak_ptr<MessageHandler>;

			using ConnectionHandler = PreFn<WebSocketConnectionArgs &>;
			using ConnectionHandlerPtr = std::shared_ptr<ConnectionHandler>;
			using WeakConnectionHandlerPtr = std::weak_ptr<ConnectionHandler>;

			using CloseHandler = std::function<void(Server &, Client &)>;
			using CloseHandlerPtr = std::shared_ptr<CloseHandler>;
			using WeakCloseHandlerPtr = std::weak_ptr<CloseHandler>;

		private:
			std::map<int, std::list<WeakMessageHandlerPtr>> webSocketMessageHandlers;
			std::map<int, std::list<WeakCloseHandlerPtr>> webSocketCloseHandlers;

			[[nodiscard]] static std::filesystem::path getWebRoot(const std::string &);
			[[nodiscard]] static bool validatePath(const std::string_view &);
			[[nodiscard]] static std::vector<std::string> getParts(std::string_view);

		public:
			std::shared_ptr<Algiz::Server> server;
			nlohmann::json options;
			std::filesystem::path webRoot;
			std::list<PrePtr<HandlerArgs &>> handlers;
			std::list<WeakConnectionHandlerPtr> webSocketConnectionHandlers;

			Server() = delete;
			Server(const Server &) = delete;
			Server(Server &&) = delete;
			Server(const std::shared_ptr<Algiz::Server> &, const nlohmann::json &options_);

			~Server() override;

			Server & operator=(const Server &) = delete;
			Server & operator=(Server &&) = delete;

			void run() override;
			void stop() override;
			void handleGet(Client &, const Request &);
			void handleWebSocketMessage(Client &, std::string_view);
			/** Doesn't send a close packet to the client; that should be done by the caller. */
			void closeWebSocket(Client &);
			void send400(Client &);
			void send401(Client &, std::string_view realm);
			void send403(Client &);
			void send500(Client &);
			void cleanWebSocketHandlers();
			void cleanWebSocketMessageHandlers();
			void cleanWebSocketCloseHandlers();
			void registerWebSocketMessageHandler(const Client &, const WeakMessageHandlerPtr &);
			void registerWebSocketCloseHandler(const Client &, const WeakCloseHandlerPtr &);
	};
}
