#include "Log.h"
#include "util/FS.h"

#include <cstring>
#include <fstream>
#include <grp.h>
#include <iostream>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
	bool inGroup(uid_t uid, gid_t gid) {
		struct passwd *pw = getpwuid(uid);
		if (pw == nullptr) {
			throw std::runtime_error(std::format("uid not found: {}", uid));
		}

		if (pw->pw_gid == gid) {
			return true;
		}

		struct group *grp = getgrgid(gid);
		if (grp == nullptr) {
			throw std::runtime_error(std::format("gid not found: {}", uid));
		}

		for (char **member = grp->gr_mem; *member != nullptr; ++member) {
			if (strcmp(*member, pw->pw_name) == 0) {
				return true;
			}
		}

		return false;
	}
}

namespace Algiz {
	bool isSubpath(const std::filesystem::path &base, std::filesystem::path to_check) {
		if (base == to_check) {
			return true;
		}

		const std::filesystem::path root("/");

		while (to_check != root && !to_check.empty()) {
			if (to_check == base) {
				return true;
			}

			to_check = to_check.parent_path();
		}

		return false;
	}

	std::string readFile(const std::filesystem::path &path) {
		std::ifstream stream;
		stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		stream.open(path);
		stream.exceptions(std::ifstream::goodbit);
		if (!stream.is_open())
			throw std::runtime_error("Couldn't open file for reading");
		stream.seekg(0, std::ios::end);
		std::string out;
		out.reserve(stream.tellg());
		stream.seekg(0, std::ios::beg);
		out.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		stream.close();
		return out;
	}

	std::string readFile(const std::filesystem::path &path, size_t offset, size_t byte_count) {
		std::ifstream stream;
		stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		stream.open(path);
		stream.exceptions(std::ifstream::goodbit);
		if (!stream.is_open())
			throw std::runtime_error("Couldn't open file for reading");
		stream.seekg(offset, std::ios::beg);
		char *buffer = new char[byte_count];
		try {
			stream.read(buffer, byte_count);
			stream.close();
		} catch (...) {
			delete[] buffer;
			throw;
		}

		std::string out(buffer, byte_count);
		delete[] buffer;
		return out;
	}

	std::time_t lastWritten(const std::filesystem::path &path) {
		// Hideous temporary hack.
#ifdef __APPLE__
		return std::chrono::file_clock::to_time_t(std::filesystem::last_write_time(path));
#else
		return std::chrono::system_clock::to_time_t(std::chrono::file_clock::to_sys(
			std::filesystem::last_write_time(path)));
#endif
	}

	bool canExecute(const std::filesystem::path &path) {
		std::filesystem::file_status status = std::filesystem::status(path);
		std::filesystem::perms perms = status.permissions();

		using enum std::filesystem::perms;

		if ((perms & owner_exec) != none && (perms & group_exec) != none && (perms & others_exec) != none) {
			return true;
		}

		if (!((perms & owner_exec) != none || (perms & group_exec) != none || (perms & others_exec) != none)) {
			return false;
		}

		struct stat info = getInfo(path);

		if (selfOwns(info)) {
			return (perms & owner_exec) != none;
		}

		if (groupOwns(info)) {
			return (perms & group_exec) != none;
		}

		return (perms & others_exec) != none;
	}

	struct stat getInfo(const std::filesystem::path &path) {
		struct stat info{};

		if (0 != stat(path.c_str(), &info)) {
			throw std::runtime_error(std::format("stat failed: {}", errno));
		}

		return info;
	}

	bool selfOwns(const std::filesystem::path &path) {
		return selfOwns(getInfo(path));
	}

	bool selfOwns(const struct stat &info) {
		return info.st_uid == getuid();
	}

	bool groupOwns(const std::filesystem::path &path) {
		return groupOwns(getInfo(path));
	}

	bool groupOwns(const struct stat &info) {
		return inGroup(getuid(), info.st_gid);
	}

	bool isNewerThan(const std::filesystem::path &check, const std::filesystem::path &basis) {
		auto check_time = std::filesystem::last_write_time(check);
		auto basis_time = std::filesystem::last_write_time(basis);
		return check_time > basis_time;
	}
}
