#include "http/Client.h"
#include "http/Response.h"
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
		if (!enabled || !acmeClient) {
			return CancelableResult::Pass;
		}

		auto &[http, client, request, parts] = args;
		auto &server = http.server;
		auto id = client.id;

		try {
			if (request.path.starts_with("/.well-known/acme-challenge/")) {
				std::unique_lock lock{challengesMutex};
				if (auto iter = challenges.find(request.path); iter != challenges.end()) {
					std::string challenge = std::move(iter->second);
					challenges.erase(iter);
					lock.unlock();
					INFO("Serving ACME challenge for " << request.path);
					client.send(HTTP::Response(200, iter->second));
					return CancelableResult::Approve;
				}
				lock.unlock();
				client.send(HTTP::Response(404, "Not Found"));
				client.close();
				return CancelableResult::Kill;
			}

			if (acmeClient && request.path == "/le-renew") {
				std::string_view host = request.getHeader("host");

				if (host.empty()) {
					client.send(HTTP::Response(403, "Forbidden"));
					client.close();
					return CancelableResult::Kill;
				}

				acme_lw::Certificate cert = acmeClient->issueCertificate({std::string(host)}, [&](const std::string &domain, const std::string &key, const std::string &authorization) {
					if (domain != host) {
						throw std::runtime_error(std::format("Mismatch between host {{{}}} and domain {{{}}}", host, domain));
					}

					std::string prefix = std::format("http://{}/", host);

					if (!key.starts_with(prefix)) {
						throw std::runtime_error(std::format("Key parameter {{{}}} doesn't begin with expected {{{}}}", key, prefix));
					}

					std::unique_lock lock{challengesMutex};
					challenges.emplace(key.substr(prefix.size() - 1), authorization);
					INFO("Added challenge for " << key);
				}, acme_lw::AcmeClient::Challenge::HTTP);

				finishCert(host, std::move(cert));

				INFO("Finished challenge for " << host << '.');
				client.send(HTTP::Response(200, "Renewed."));
				return CancelableResult::Approve;
			}
		} catch (const std::exception &err) {
			ERROR(err.what());
			client.send(HTTP::Response(403, "Forbidden"));
			client.close();
			return CancelableResult::Kill;
		} catch (...) {
			ERROR("Unknown error occurred in letsencrypt plugin");
			client.send(HTTP::Response(403, "Forbidden"));
			client.close();
			return CancelableResult::Kill;
		}

		return CancelableResult::Pass;
	}

	void LetsEncrypt::finishCertificate(std::string_view host, acme_lw::Certificate certificate) {

	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::LetsEncrypt;
}
