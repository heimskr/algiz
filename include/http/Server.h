#pragma once

#include "ApplicationServer.h"
#include "http/Request.h"
#include "net/Server.h"
#include "nlohmann/json.hpp"
#include "plugins/PluginHost.h"
#include "util/FS.h"
#include "util/StringVector.h"
#include "util/WeakCompare.h"
#include "wahtwo/Watcher.h"

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace Algiz::HTTP {
	class Client;

	class Server: public ApplicationServer, public Plugins::PluginHost {
		public:
			struct HandlerArgs {
				Server &server;
				Client &client;
				Request request;
				const StringVector parts;

				explicit HandlerArgs(Server &server, Client &client, Request request, StringVector parts):
					server(server),
					client(client),
					request(std::move(request)),
					parts(std::move(parts)) {}
			};

			struct WebSocketConnectionArgs: HandlerArgs {
				/** The list of protocols requested by the client. The server should pick exactly one of these.
				 *  May be empty. */
				const StringVector protocols;

				/** The protocol accepted by the server. Sent to the client with the Sec-WebSocket-Protocol header.
				 *  Expected to be changed by connection handlers. */
				std::string acceptedProtocol;

				explicit WebSocketConnectionArgs(Server &server, Client &client, Request request, StringVector parts, StringVector protocols):
					HandlerArgs(server, client, std::move(request), std::move(parts)),
					protocols(std::move(protocols)) {}
			};

			struct WebSocketMessageArgs {
				Server &server;
				Client &client;
				std::string_view message;

				explicit WebSocketMessageArgs(Server &server, Client &client, std::string_view message):
					server(server),
					client(client),
					message(message) {}
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
			std::optional<Wahtwo::Watcher> watcher;
			std::thread watcherThread;
			std::mutex configsMutex;
			bool dying = false;

			[[nodiscard]] static std::filesystem::path getWebRoot(const std::string &);
			[[nodiscard]] static bool validatePath(const std::string_view &);
			[[nodiscard]] static std::vector<std::string> getParts(std::string_view);

		public:
			std::shared_ptr<Algiz::Server> server;
			nlohmann::json options;
			std::filesystem::path webRoot;
			std::list<WeakPrePtr<HandlerArgs &>> getHandlers;
			std::list<WeakPrePtr<HandlerArgs &>> postHandlers;
			std::list<WeakConnectionHandlerPtr> webSocketConnectionHandlers;
			std::map<std::filesystem::path, nlohmann::json> configs;

			Server() = delete;
			Server(const Server &) = delete;
			Server(Server &&) = delete;
			Server(const std::shared_ptr<Algiz::Server> &, const nlohmann::json &options_);

			~Server() override;

			Server & operator=(const Server &) = delete;
			Server & operator=(Server &&) = delete;

			void run() override;
			void stop() override;
			void handleGET(Client &, const Request &);
			void handlePOST(Client &, const Request &);
			void handleWebSocketMessage(Client &, std::string_view);
			/** Doesn't send a close packet to the client; that should be done by the caller. */
			void closeWebSocket(Client &);
			void send400(Client &);
			void send401(Client &, std::string_view realm);
			void send401(Client &);
			void send403(Client &);
			void send500(Client &);
			void cleanWebSocketHandlers();
			void cleanWebSocketMessageHandlers();
			void cleanWebSocketCloseHandlers();
			void registerWebSocketMessageHandler(const Client &, const WeakMessageHandlerPtr &);
			void registerWebSocketCloseHandler(const Client &, const WeakCloseHandlerPtr &);

			auto lockConfigs() { return std::unique_lock(configsMutex); }

			template <typename T, typename N>
			T & getOption(const N &name) {
				return options.at(name).template get_ref<T &>();
			}

			template <typename T = nlohmann::json, typename N>
			std::optional<T> getOption(std::string_view web_path, const N &name) {
				if (!web_path.empty() && web_path.front() == '/') {
					web_path.remove_prefix(1);
				}

				return getOption(webRoot / web_path, name);
			}

			template <typename T = nlohmann::json, typename N>
			std::optional<T> getOption(const std::string &web_path, const N &name) {
				return getOption(std::string_view(web_path), name);
			}

			template <typename T = nlohmann::json, typename N>
			std::optional<T> getOption(const std::filesystem::path &path, const N &name) {
				if (!isSubpath(webRoot, path)) {
					throw std::invalid_argument("Not a subpath of the webroot (" + webRoot.string() + "): " + path.string());
				}

				auto dir = path.parent_path();
				const auto root = dir.root_path();

				do {
					auto lock = lockConfigs();
					if (configs.contains(dir)) {
						auto &config = configs.at(dir);
						if (config.contains(name)) {
							if constexpr (std::is_same_v<T, nlohmann::json>) {
								return config.at(name);
							} else {
								return config.at(name).template get<T>();
							}
						}
					}

					if (dir == webRoot) {
						break;
					}

					dir = dir.parent_path();
				} while (dir != root);

				if (options.contains(name)) {
					if constexpr (std::is_same_v<T, nlohmann::json>) {
						return options.at(name);
					} else {
						return options.at(name).template get<T>();
					}
				}

				return std::nullopt;
			}

		private:
			void addConfig(const std::filesystem::path &);
			static decltype(configs) crawlConfigs(const std::filesystem::path &base);
			static void crawlConfigs(const std::filesystem::path &base, decltype(configs) &);
	};
}
