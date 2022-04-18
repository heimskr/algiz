#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <unordered_set>

#include "net/GenericClient.h"

namespace Algiz {
	class Server {
		private:
			int af;
			std::string ip;
			int makeSocket();
			int port;
			size_t chunkSize;
			char *buffer;
			int lastClient = -1;
			int sock = -1;
			fd_set activeSet;
			int controlRead = -1, controlWrite = -1;
			bool connected = false;

			enum class ControlMessage: char {Close='C'};

			/** Maps client IDs to descriptors. */
			std::map<int, int> descriptors;

			/** Maps descriptors to client IDs. */
			std::map<int, int> clients;

			/** Maps descriptors to buffers. */
			std::map<int, std::string> buffers;

			/** Contains IDs that were previously used but are now available. */
			std::unordered_set<int> freePool;

		protected:
			std::unordered_map<int, std::unique_ptr<GenericClient>> allClients;

			virtual void addClient(int) = 0;

		public:
			std::function<void(int, const std::string &)> messageHandler; // (int client, const std::string &message)
			std::function<void(int, int)> onEnd; // (int client, int descriptor)

			Server(int af_, const std::string &ip_, uint16_t port_, size_t chunk_size = 1);
			virtual ~Server();

			int getPort() const { return port; }
			void readFromClient(int descriptor);
			virtual void handleMessage(int client, const std::string &message);
			virtual void end(int descriptor);
			void send(int client, const std::string &message, bool suppress_newline = false);
			void removeClient(int);
			void run();
			void stop();
			decltype(allClients) & getClients() { return allClients; }
			const decltype(allClients) & getClients() const { return allClients; }

			/** Given a buffer, this function returns {-1, *} if the message is still incomplete or the {i, l} if the
			 *  buffer contains a complete message, where i is the index at which the message ends and l is the size of
			 *  the delimiter that ended the message. By default, a message is considered complete after the first
			 *  newline. */
			virtual std::pair<ssize_t, size_t> isMessageComplete(const std::string &);
	};
}
