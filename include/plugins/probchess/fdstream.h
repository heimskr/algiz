// The following code is based on code from the book "The C++ Standard Library - A Tutorial and Reference"
// by Nicolai M. Josuttis, Addison-Wesley, 1999. © Copyright Nicolai M. Josuttis 1999

#include <cstdio>
#include <cstring>
#include <iostream>
#include <streambuf>

class ifdbuf: public std::streambuf {
	protected:
		int fd;
		static constexpr int bufferSize = 10;
		char buffer[bufferSize];

		// Insert new characters into the buffer.
		virtual int_type underflow() {
			// Is read position before end of buffer?
			if (gptr() < egptr()) {
				return traits_type::to_int_type(*gptr());
			}

			// Process size of putback area. Use number of characters read, but at most four.
			int numPutback = gptr() - eback();
			if (numPutback > 4)
				numPutback = 4;

			// Copy up to four characters previously read into the putback buffer (area of first four characters).
			std::memmove(buffer + (4 - numPutback), gptr() - numPutback, numPutback);

			// Read new characters.
			int num = read(fd, buffer + 4, bufferSize - 4);
			if (num <= 0) {
				// ERROR or EOF
				return EOF;
			}

			// Reset buffer pointers.
			setg(buffer+(4-numPutback), // beginning of putback area
			     buffer+4,              // read position
			     buffer+4+num);         // end of buffer

			// Return next character.
			return traits_type::to_int_type(*gptr());
		}

	public:
		ifdbuf(int fd_): fd(fd_) {
			setg(buffer+4,  // beginning of putback area
			     buffer+4,  // read position
			     buffer+4); // end position
		}
};

class ifdstream: public std::istream {
	protected:
		ifdbuf buf;

	public:
		ifdstream(int fd): std::istream(0), buf(fd) {
			rdbuf(&buf);
		}
};

class ofdbuf: public std::streambuf {
	protected:
		int fd;

		// Write one character.
		virtual int_type overflow(int_type c) {
			if (c != EOF) {
				char z = c;
				if (write(fd, &z, 1) != 1) {
					return EOF;
				}
			}
			return c;
		}

		// Write multiple characters.
		virtual std::streamsize xsputn(const char *s, std::streamsize num) {
			return write(fd, s, num);
		}

	public:
		ofdbuf(int fd_): fd(fd_) {}
};

class ofdstream: public std::ostream {
	protected:
		ofdbuf buf;

	public:
		ofdstream(int fd): std::ostream(0), buf(fd) {
			rdbuf(&buf);
		}
};
