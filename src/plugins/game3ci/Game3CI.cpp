#include "Log.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Game3CI.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Shell.h"
#include "util/Templates.h"
#include "util/Util.h"

#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <openssl/hmac.h>

#include <array>
#include <chrono>
#include <string>

static std::string hmacSHA256(std::string_view decoded_key, std::string_view message) {
	// https://stackoverflow.com/a/64570079
	std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
	unsigned int hash_length;
	HMAC(EVP_sha256(), decoded_key.data(), static_cast<int>(decoded_key.size()), reinterpret_cast<const uint8_t *>(message.data()), static_cast<int>(message.size()), hash.data(), &hash_length);
	return {reinterpret_cast<const char *>(hash.data()), hash_length};
}

static bool compareHMAC(std::string_view one, std::string_view two) {
	// I hope this is good enough.

	if (one.size() != two.size()) {
		throw std::runtime_error("Strings must have same length to be fully compared");
	}

	int sum = one.size();

	for (size_t i = 0; i < one.size(); ++i) {
		if (one[i] == two[i]) {
			--sum;
		} else {
			++sum;
		}
	}

	return sum == 0;
}

namespace Algiz::Plugins {
	void Game3CI::postinit(PluginHost *host) {
		repoRoot = config.at("repo");
		builddir = config.at("builddir");
		secret = config.at("secret");
		if (auto iter = config.find("quasiMsys2"); iter != config.end()) {
			quasiMsys2 = *iter;
		}
		if (auto iter = config.find("force"); iter != config.end()) {
			force = *iter;
		}
		pool.start();
		dynamic_cast<HTTP::Server &>(*(parent = host)).postHandlers.emplace_back(handler);
	}

	void Game3CI::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).postHandlers, handler);
		pool.detach();
	}

	CancelableResult Game3CI::handle(const HTTP::Server::HandlerArgs &args, bool not_disabled) {
		if (!not_disabled) {
			return CancelableResult::Pass;
		}

		const auto &[http, client, request, parts] = args;

		if (request.path != "/ci/hook") {
			return CancelableResult::Pass;
		}

		if (request.getHeader("content-type") != "application/json") {
			client.send(HTTP::Response(400, "What?", "text/plain"));
			client.close();
			return CancelableResult::Kill;
		}

		std::string signature(request.getHeader("x-hub-signature-256").substr(7));
		std::string expected = hexString(hmacSHA256(secret, request.content));
		signature.resize(expected.size());

		if (!compareHMAC(signature, expected)) {
			client.send(HTTP::Response(403, "No.", "text/plain"));
			client.close();
			return CancelableResult::Kill;
		}

		nlohmann::json json;
		try {
			json = nlohmann::json::parse(request.content);
		} catch (...) {
			client.send(HTTP::Response(400, "Bad JSON", "text/plain"));
			client.close();
			return CancelableResult::Kill;
		}

		if (auto ref_iter = json.find("ref"); ref_iter != json.end() && *ref_iter == "refs/heads/master") {
			if (auto after_iter = json.find("after"); after_iter != json.end()) {
				pool.add(makeJob(*after_iter));
			}
		}

		client.send(HTTP::Response(200, "Thanks!", "text/plain"));
		client.close();
		return CancelableResult::Approve;
	}

	template <typename T>
	concept Stringable = std::constructible_from<std::string, T>;

	ThreadPool::Function Game3CI::makeJob(std::string commit_hash) {
		return [repo_root = repoRoot, build_dir = builddir, force = force, quasi_msys2 = quasiMsys2, commit_hash = std::move(commit_hash)](ThreadPool &, size_t) -> void {
			INFO("Getting to work on commit " << commit_hash << "...");

			const std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

			int code{}, signal{};
			std::string out, err;

			auto run = [&](const std::string &command, Stringable auto &&...args) -> bool {
				std::vector<std::string> vector;
				vector.reserve(sizeof...(args));
				(vector.emplace_back(args), ...); // lack of forwarding is intentional
				auto result = runCommand(command, vector);
				out = std::move(result.out);
				err = std::move(result.err);
				code = result.code;
				signal = result.signal;
				if (code != 0) {
					ERROR("Command " << command << ((" " + std::string(args)) + ...) << " failed (code " << code << ", signal " << signal << ")");
					ERROR(err);
					return false;
				}
				return true;
			};

			if (!run("git", "-C", build_dir, "pull", "origin", "master")) {
				ERROR("Couldn't pull from repo.");
				return;
			}

			if (!run("git", "-C", build_dir, "checkout", commit_hash)) {
				ERROR("Couldn't check out commit " << commit_hash << '.');
				return;
			}

			if (!run("ninja", "-C", build_dir)) {
				ERROR("Failed to build.");
				return;
			}

			if (!force && out.contains("ninja: no work to do.\n")) {
				WARN("No work to do for commit " << commit_hash << '.');
				return;
			}

			INFO("Starting Linux build for commit " << commit_hash << "...");

			if (!run(repo_root + "/linzip.sh", repo_root, build_dir, "publish")) {
				ERROR("Failed to produce and publish a Linux zip.");
				return;
			}

			if (!quasi_msys2.empty()) {
				INFO("Starting Windows build for commit " << commit_hash << "...");
				if (!run(repo_root + "/quasi_msys2.sh", repo_root, quasi_msys2, "publish")) {
					ERROR("Failed to produce and publish a Windows zip.");
					return;
				}
			}

			double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() / 1000.0;
			SUCCESS("Published commit " << commit_hash << " in " << seconds << " seconds.");
		};
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Game3CI;
}
