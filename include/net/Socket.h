#pragma once

#include <netdb.h>
#include <string>

namespace Algiz {
	class Socket {
		friend class SocketBuffer;

		protected:
			static int socketCount;
			struct addrinfo *info = nullptr;
			int netFD = -1, controlRead = -1, controlWrite = -1;
			bool connected = false;
			fd_set fds = {0};

			enum class ControlMessage: char {Close='C'};

		public:
			std::string hostname;
			int port;

			Socket(const std::string &hostname_, int port_);

			Socket() = delete;
			Socket(const Socket &) = delete;
			Socket & operator=(const Socket &) = delete;
			Socket & operator=(Socket &&) noexcept;

			~Socket();

			/** Connects to the socket. */
			virtual void connect();

			/** Closes the socket. */
			void close();

			/** Sends a given number of bytes from a buffer through the socket and returns the number of bytes sent. */
			virtual ssize_t send(const void *, size_t);

			/** Reads a given number of bytes into a buffer from the socket and returns the number of bytes read. */
			virtual ssize_t recv(void *, size_t);

			int accept();

			template <typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0,
			                      std::enable_if_t<!std::is_same<T, bool>::value, int> = 0>
			Socket & operator<<(T num) {
				return *this << std::to_string(num);
			}

			Socket & operator<<(const std::string_view &view) {
				send(view.data(), view.size());
				return *this;
			}

			Socket & operator<<(const char *str) {
				return *this << std::string(str);
			}
	};
}
