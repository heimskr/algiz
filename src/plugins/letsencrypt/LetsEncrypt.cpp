#include "http/Client.h"
#include "http/Response.h"
#include "plugins/LetsEncrypt.h"
#include "util/FS.h"
#include "Core.h"
#include "Log.h"

#include <fstream>

namespace {
	constexpr size_t MAX_CHALLENGES = 128;
}

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
			acme_lw::AcmeClient::Environment env = acme_lw::AcmeClient::Environment::STAGING;
			acme_lw::AcmeClient::init(env);
			acmeClient.emplace(account_key);
			std::memset(account_key.data(), 0, account_key.size());
			account_key.clear();
		} catch (...) {
			acme_lw::AcmeClient::teardown();
			throw;
		}

		if (auto ssl = getSSLServer()) {
			oldRequestCertificate = ssl->requestCertificate.swap([this](const char *servername) {
				pool.add([this, host = std::string(servername)](ThreadPool &, size_t) {
					issueCertificate(host);
				});
			});
		}

		pool.start();
	}

	void LetsEncrypt::cleanup(PluginHost *host) {
		PluginHost::erase(dynamic_cast<HTTP::Server &>(*host).getHandlers, handler);

		if (oldRequestCertificate) {
			if (auto ssl = getSSLServer()) {
				ssl->requestCertificate = std::move(*oldRequestCertificate);
			}
		}

		pool.join();

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
					INFO("Serving ACME challenge for " << request.path << ": " << iter->second);
					client.send(HTTP::Response(200, iter->second));
					return CancelableResult::Approve;
				}
				lock.unlock();
				WARN("Couldn't find challenge for " << request.path);
				client.send(HTTP::Response(404, "Not Found"));
				client.close();
				return CancelableResult::Kill;
			}

			if (acmeClient && request.path == "/le-renew") {
				std::string host(request.getHeader("host"));

				if (host.empty()) {
					client.send(HTTP::Response(403, "Forbidden"));
					client.close();
					return CancelableResult::Kill;
				}

				pool.add([this, host = std::move(host)](ThreadPool &, size_t) {
					issueCertificate(host);
				});

				client.send(HTTP::Response(200, "Pending."));
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

	void LetsEncrypt::issueCertificate(const std::string &host) {
		acme_lw::Certificate cert = acmeClient->issueCertificate({host}, [&](const std::string &domain, const std::string &key, const std::string &authorization) {
			if (domain != host) {
				throw std::runtime_error(std::format("Mismatch between host {{{}}} and domain {{{}}}", host, domain));
			}

			std::string prefix = std::format("http://{}/", host);

			if (!key.starts_with(prefix)) {
				throw std::runtime_error(std::format("Key parameter {{{}}} doesn't begin with expected {{{}}}", key, prefix));
			}

			std::unique_lock lock{challengesMutex};
			auto [iter, inserted] = challenges.emplace(key.substr(prefix.size() - 1), authorization);
			if (inserted) {
				challengeIterators.push_back(iter);
				while (challengeIterators.size() > MAX_CHALLENGES) {
					challenges.erase(challengeIterators.front());
					challengeIterators.pop_front();
				}
			}
		}, acme_lw::AcmeClient::Challenge::HTTP);

		finishCertificate(host, std::move(cert));
		INFO("Finished challenge for " << host << '.');
	}

	void LetsEncrypt::finishCertificate(std::string_view host, acme_lw::Certificate certificate) {
		if (auto ssl = getSSLServer()) {
			std::string first_cert(certificate.fullchain);
			std::string_view end = "-----END CERTIFICATE-----";
			size_t position = first_cert.find(end);
			if (position == std::string::npos) {
				throw std::runtime_error("Couldn't find end of first fullchain cert");
			}
			first_cert.erase(first_cert.begin() + position + end.size(), first_cert.end());
			ssl->addCertificate(std::string(host), first_cert, certificate.privkey);
			INFO("Saved certificate for " << host);
		} else {
			WARN("No HTTPS server to give the certificate for " << host);
		}
	}

	std::shared_ptr<SSLServer> LetsEncrypt::getSSLServer() const {
		return dynamic_cast<HTTP::Server &>(*parent).server->getCore().getSSLServer();
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::LetsEncrypt;
}
