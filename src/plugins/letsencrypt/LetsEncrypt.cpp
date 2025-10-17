#include "http/Client.h"
#include "plugins/LetsEncrypt.h"
#include "util/FS.h"
#include "Log.h"

namespace Algiz::Plugins {
	void LetsEncrypt::postinit(PluginHost *host) {
		dynamic_cast<HTTP::Server &>(*(parent = host)).getHandlers.push_back(handler);

		std::string account_key;

		if (auto iter = config.find("accountKey"); iter != config.end() || !iter->is_string()) {
			account_key = readFile(*iter);
		} else {
			throw std::out_of_range("Plugin letsencrypt requires an accountKey string option");
		}

		try {
			acme_lw::AcmeClient::Environment env = acme_lw::AcmeClient::Environment::PRODUCTION;
			acme_lw::AcmeClient::init(env);
			acmeClient.emplace(account_key);
			std::memset(account_key.data(), 0, account_key.size());
			account_key.clear();
		} catch (...) {
			acme_lw::AcmeClient::teardown();
			throw;
		}
	}

	void LetsEncrypt::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, handler);

		if (acmeClient) {
			acmeClient.reset();
			acme_lw::AcmeClient::teardown();
		}
	}

	CancelableResult LetsEncrypt::handle(const HTTP::Server::HandlerArgs &args, bool enabled) {
		if (!enabled) {
			return CancelableResult::Pass;
		}

		auto &[http, client, request, parts] = args;
		auto &server = http.server;
		auto id = client.id;

		return CancelableResult::Pass;
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::LetsEncrypt;
}
