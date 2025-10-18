#pragma once

#include <openssl/ssl.h>

namespace Algiz {
	struct Certificates {
		X509 *certificate;
		EVP_PKEY *privateKey;

		void free() {
			X509_free(certificate);
			EVP_PKEY_free(privateKey);
			certificate = nullptr;
			privateKey = nullptr;
		}
	};
}
