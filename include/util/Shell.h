#pragma once

#include <chrono>
#include <span>
#include <string>

namespace Algiz {
	struct CommandOutput {
		std::string out;
		std::string err;
		int code = -1;
		int signal = -1;

		CommandOutput() = default;
		CommandOutput(std::string out, std::string err, int code, int signal):
			out(std::move(out)),
			err(std::move(err)),
			code(code),
			signal(signal) {}
	};

	CommandOutput runCommand(const std::string &path, std::span<const std::string> args);
	CommandOutput runCommand(const std::string &path, std::span<const std::string> args, std::chrono::microseconds timeout, int signal_on_timeout = 0);

	template <typename Duration>
	CommandOutput runCommand(const std::string &path, std::span<const std::string> args, Duration duration, int signal_on_timeout = 0) {
		return runCommand(path, args, std::chrono::duration_cast<std::chrono::microseconds>(duration), signal_on_timeout);
	}

	template <typename... Args>
	CommandOutput runCommand(const std::string &path, std::initializer_list<std::string> args, Args &&...extra) {
		return runCommand(path, std::span<const std::string>(args.begin(), args.size()), std::forward<Args>(extra)...);
	}
}
