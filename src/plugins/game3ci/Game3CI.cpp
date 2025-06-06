#include "Log.h"
#include "http/Client.h"
#include "http/Response.h"
#include "http/Server.h"
#include "plugins/Game3CI.h"
#include "util/FS.h"
#include "util/MIME.h"
#include "util/Templates.h"
#include "util/Util.h"

#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <openssl/hmac.h>

#include <string>
#include <array>

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

	bool out = true;

	for (size_t i = 0; i < one.size(); ++i) {
		if (one[i] != two[i]) {
			out = false;
		}
	}

	return out;
}

namespace Algiz::Plugins {
	void Game3CI::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).postHandlers.emplace_back(handler);
	}

	void Game3CI::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).postHandlers, handler);
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
		std::string expected = hex(hmacSHA256(std::string(config.at("secret")), request.content));
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

		[&] {
			if (json["ref"] != "refs/heads/master") {
				return;
			}
		}();

		client.send(HTTP::Response(200, "Thanks!", "text/plain"));
		client.close();
		return CancelableResult::Approve;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::Game3CI;
}
