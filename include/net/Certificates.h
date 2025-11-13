#pragma once

#include <openssl/ssl.h>

namespace Algiz {
	struct Certificates {
		X509 *certificate = nullptr;
		EVP_PKEY *privateKey = nullptr;
		std::vector<X509 *> chain;

		~Certificates() {
			if (certificate != nullptr) {
				X509_free(certificate);
			}

			if (privateKey != nullptr) {
				EVP_PKEY_free(privateKey);
			}

			for (X509 *x509: chain) {
				X509_free(x509);
			}

			certificate = nullptr;
			privateKey = nullptr;
			chain.clear();
		}
	};
}
