#include "http/Client.h"
#include "http/Response.h"
#include "plugins/LetsEncrypt.h"
#include "util/FS.h"
#include "Core.h"
#include "Log.h"

#include <fstream>

namespace {
	constexpr size_t MAX_CHALLENGES = 128;
	const std::filesystem::path CERTS_PATH = "certs";

	void ensureDirectory() {
		if (!std::filesystem::exists(CERTS_PATH)) {
			std::filesystem::create_directory(CERTS_PATH);
		}
	}
}

namespace Algiz::Plugins {
	std::unordered_map<std::string, std::string> LetsEncrypt::challenges;
	std::deque<decltype(LetsEncrypt::challenges)::iterator> LetsEncrypt::challengeIterators;
	std::mutex LetsEncrypt::challengesMutex;

	void LetsEncrypt::postinit(PluginHost *host) {
		auto &http = dynamic_cast<HTTP::Server &>(*(parent = host));
		http.getHandlers.push_back(handler);

		std::string account_key;

		if (auto iter = config.find("accountKey"); iter != config.end() || !iter->is_string()) {
			account_key = readFile(*iter);
		} else {
			throw std::out_of_range("Plugin letsencrypt requires an accountKey string option");
		}

		if (auto iter = config.find("whitelist"); iter != config.end()) {
			whitelist = iter->get<decltype(whitelist)::value_type>();
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

		if (auto ssl = std::dynamic_pointer_cast<SSLServer>(http.server)) {
			loadCache();
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

		try {
			if (request.path.starts_with("/.well-known/acme-challenge/")) {
				std::unique_lock lock{challengesMutex};
				if (auto iter = challenges.find(request.path); iter != challenges.end()) {
					client.send(HTTP::Response(200, iter->second));
					return CancelableResult::Approve;
				}
				lock.unlock();
				client.send(HTTP::Response(404, "Not Found"));
				client.close();
				return CancelableResult::Kill;
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
		if (whitelist && !whitelist->contains(host)) {
			return;
		}

		try {
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
		} catch (const acme_lw::AcmeException &error) {
			WARN("Issuing certificate failed for " << host << ": " << error.what());
		}
	}

	void LetsEncrypt::finishCertificate(std::string_view host, acme_lw::Certificate certificate) {
		std::shared_ptr<SSLServer> ssl = getSSLServer();
		if (!ssl) {
			WARN("No HTTPS server to give the certificate for " << host);
			return;
		}

		std::string first_cert(certificate.fullchain);
		std::string_view end = "-----END CERTIFICATE-----";
		size_t position = first_cert.find(end);
		if (position == std::string::npos) {
			throw std::runtime_error("Couldn't find end of first fullchain cert");
		}
		first_cert.erase(first_cert.begin() + position + end.size(), first_cert.end());
		std::string rest_of_chain = certificate.fullchain.substr(first_cert.size());
		ssl->addCertificate(std::string(host), first_cert, certificate.privkey, rest_of_chain);

		if (host.contains('/')) {
			WARN("Refusing to cache certificate for " << host);
			INFO("Registered certificate for " << host);
			return;
		}

		ensureDirectory();
		std::filesystem::path path = CERTS_PATH / host;
		path += ".json";
		std::ofstream(path) << nlohmann::json{
			{"expiry", certificate.getExpiry()},
			{"hostname", host},
			{"cert", first_cert},
			{"privkey", certificate.privkey},
			{"chain", rest_of_chain},
		}.dump();
		INFO("Registered and cached certificate for " << host);
	}

	void LetsEncrypt::loadCache() {
		std::shared_ptr<SSLServer> ssl = getSSLServer();

		if (!ssl || !std::filesystem::exists(CERTS_PATH)) {
			return;
		}

		for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(CERTS_PATH)) {
			std::filesystem::path path = entry.path();
			if (path.extension() == ".json") {
				nlohmann::json json = nlohmann::json::parse(readFile(path));
				std::string hostname = json.at("hostname");
				time_t expiry = json.at("expiry");
				if (expiry <= time(nullptr)) {
					WARN("Ignoring stale certificate for " << hostname);
					continue;
				}

				std::string cert = json.at("cert");
				std::string privkey = json.at("privkey");
				std::string rest_of_chain = json.at("chain");
				ssl->addCertificate(hostname, cert, privkey, rest_of_chain);
				SUCCESS("Added cached certificate for " << hostname);
			}
		}
	}

	std::shared_ptr<SSLServer> LetsEncrypt::getSSLServer() const {
		return dynamic_cast<HTTP::Server &>(*parent).server->getCore().getSSLServer();
	}
}

extern "C" Algiz::Plugins::Plugin * make_plugin() {
	return new Algiz::Plugins::LetsEncrypt;
}
