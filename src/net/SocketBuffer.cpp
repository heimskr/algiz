#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include "net/Socket.h"
#include "net/SocketBuffer.h"
#include "net/NetError.h"
#include "Log.h"

namespace Algiz {
	SocketBuffer::SocketBuffer(Socket *source_, size_t buffer_size, size_t putbackSize):
	source(source_), bufferSize(buffer_size), putbackSize(putbackSize) {
		buffer = new char[bufferSize];
		setg(buffer + putbackSize, buffer + putbackSize, buffer + putbackSize);
	}

	SocketBuffer::~SocketBuffer() {
		delete[] buffer;
	}

	std::streambuf::int_type SocketBuffer::overflow(std::streambuf::int_type byte) {
		if (!traits_type::eq_int_type(byte, traits_type::eof())) {
			try {
				source->send(&byte, 1);
			} catch (const NetError &err) {
				log << "Network error in overflow(): " << err.what() << std::endl;
				return EOF;
			}
		}

		return traits_type::not_eof(byte);
	}

	std::streamsize SocketBuffer::xsputn(const char *src, std::streamsize size) {
		try {
			return source->send(src, size);
		} catch (const NetError &err) {
			log << "Network error in xsputn(): " << err.what() << std::endl;
			return EOF;
		}
	}

	std::streambuf::int_type SocketBuffer::underflow() {
		if (gptr() < egptr())
			return traits_type::to_int_type(*gptr());

		size_t putback = std::min(putbackSize, static_cast<size_t>(gptr() - eback()));
		std::memmove(buffer + putbackSize - putback, gptr() - putback, putback);

		ssize_t bytes_read;
		try {
			bytes_read = source->recv(buffer + putbackSize, bufferSize - putbackSize);
		} catch (const NetError &err) {
			log << "Network error in underflow(): " << err.what() << std::endl;
			return EOF;
		}

		if (bytes_read == 0)
			return EOF;

		setg(buffer + putbackSize - putback, buffer + putbackSize, buffer + putbackSize + bytes_read);
		return traits_type::to_int_type(*gptr());
	}

	void SocketBuffer::close() {
		source->close();
	}
}
