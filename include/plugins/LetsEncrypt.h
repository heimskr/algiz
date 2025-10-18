#pragma once

#include "http/Server.h"
#include "net/SSLServer.h"
#include "plugins/Plugin.h"
#include "threading/ThreadPool.h"
#include "util/Util.h"

#include "acme-lw.h"

#include <deque>
#include <mutex>

namespace Algiz::Plugins {
	class LetsEncrypt: public Plugin {
		public:
			[[nodiscard]] std::string getName()        const override { return "LetsEncrypt"; }
			[[nodiscard]] std::string getDescription() const override { return "Automatically manages SSL certificates."; }
			[[nodiscard]] std::string getVersion()     const override { return "0.0.1"; }

			void postinit(PluginHost *) override;
			void cleanup(PluginHost *) override;

			std::shared_ptr<PluginHost::PreFn<HTTP::Server::HandlerArgs &>> handler =
				std::make_shared<PluginHost::PreFn<HTTP::Server::HandlerArgs &>>(bind(*this, &LetsEncrypt::handle));

		private:
			ThreadPool pool{4};
			std::optional<acme_lw::AcmeClient> acmeClient;
			std::optional<decltype(SSLServer::requestCertificate)::Base> oldRequestCertificate;

			Plugins::CancelableResult handle(const HTTP::Server::HandlerArgs &, bool not_disabled);
			void issueCertificate(const std::string &host);
			void finishCertificate(std::string_view host, acme_lw::Certificate);
			std::shared_ptr<SSLServer> getSSLServer() const;

			std::unordered_map<std::string, std::string> challenges;
			std::deque<decltype(challenges)::iterator> challengeIterators;
			std::mutex challengesMutex;
	};
}
