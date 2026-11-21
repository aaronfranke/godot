#include "drivers/win_legacy/nt/os/os_windows_nt.h"

// Godot includes.
#include "core/crypto/crypto_core.h"
#include "core/string/string_builder.h"

#include <wincrypt.h>

String OS_WindowsNT::get_system_ca_certificates() {
	constexpr DWORD FLAGS = CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG | CERT_STORE_OPEN_EXISTING_FLAG;
	constexpr DWORD CERT_NOT_FOUND = static_cast<DWORD>(CRYPT_E_NOT_FOUND);
	constexpr int PEM_LINE_WIDTH = 64;
	const HCERTSTORE roots = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, FLAGS, L"ROOT");
	if (!roots) {
		return String();
	}
	const HCERTSTORE disallowed = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, FLAGS, L"Disallowed");
	if (!disallowed) {
		const DWORD error = GetLastError();
		if (error != CERT_NOT_FOUND && error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
			CertCloseStore(roots, 0);
			return String();
		}
	}
	FILETIME now;
	GetSystemTimeAsFileTime(&now);

	StringBuilder pem;
	bool complete = true;
	PCCERT_CONTEXT certificate = nullptr;
	while ((certificate = CertEnumCertificatesInStore(roots, certificate)) != nullptr) {
		FILETIME disallowed_since;
		DWORD size = sizeof(disallowed_since);
		if (CertGetCertificateContextProperty(certificate, CERT_DISALLOWED_FILETIME_PROP_ID, &disallowed_since, &size) &&
				size == sizeof(disallowed_since) && CompareFileTime(&now, &disallowed_since) >= 0) {
			continue;
		}
		if (disallowed) {
			const PCCERT_CONTEXT match = CertFindCertificateInStore(disallowed, certificate->dwCertEncodingType, 0, CERT_FIND_EXISTING, certificate, nullptr);
			if (match) {
				CertFreeCertificateContext(match);
				continue;
			}
			if (GetLastError() != CERT_NOT_FOUND) {
				CertFreeCertificateContext(certificate);
				complete = false;
				break;
			}
		}

		const String encoded = CryptoCore::b64_encode_str(certificate->pbCertEncoded, certificate->cbCertEncoded);
		if (encoded.is_empty()) {
			continue;
		}
		pem += "-----BEGIN CERTIFICATE-----\n";
		for (int64_t i = 0; i < encoded.length(); i += PEM_LINE_WIDTH) {
			pem += encoded.substr(i, PEM_LINE_WIDTH);
			pem += "\n";
		}
		pem += "-----END CERTIFICATE-----\n";
	}
	complete = complete && GetLastError() == CERT_NOT_FOUND;
	if (disallowed) {
		CertCloseStore(disallowed, 0);
	}
	CertCloseStore(roots, 0);
	return complete ? pem.as_string() : String();
}
