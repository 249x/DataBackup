#include "DesEncryptionHandler.h"

#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace {
	constexpr std::size_t DES_KEY_SIZE = 8;
	constexpr std::size_t DES_IV_SIZE = 8;

	bool GenerateRandomIv(std::vector<std::uint8_t>& iv) {
		iv.resize(DES_IV_SIZE);
		NTSTATUS status = BCryptGenRandom(
			nullptr,
			iv.data(),
			static_cast<ULONG>(iv.size()),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG);
		return status == 0;
	}

	bool ImportKey(BCRYPT_ALG_HANDLE alg,
	               const std::vector<std::uint8_t>& key,
	               BCRYPT_KEY_HANDLE& keyHandle) {
		return BCryptGenerateSymmetricKey(
			alg,
			&keyHandle,
			nullptr,
			0,
			const_cast<PUCHAR>(key.data()),
			static_cast<ULONG>(key.size()),
			0) == 0;
	}
}

DesEncryptionHandler::DesEncryptionHandler(EncryptionManager& manager)
	: EncryptionHandler(manager) {}

DesEncryptionHandler::~DesEncryptionHandler() = default;

std::uint16_t DesEncryptionHandler::GetAlgorithmID() const {
	return AlgorithmID;
}

bool DesEncryptionHandler::Process(const std::vector<std::uint8_t>& input,
                                   const std::vector<std::uint8_t>& key,
                                   std::vector<std::uint8_t>& additionalInfo,
                                   std::vector<std::uint8_t>& output) {
	if (additionalInfo.empty()) {
		std::vector<std::uint8_t> iv;
		if (!Encrypt(input, key, iv, output)) {
			return false;
		}
		additionalInfo = std::move(iv);
		return true;
	}
	return Decrypt(input, key, additionalInfo, output);
}

bool DesEncryptionHandler::Encrypt(const std::vector<std::uint8_t>& plain,
                                   const std::vector<std::uint8_t>& key,
                                   std::vector<std::uint8_t>& iv,
                                   std::vector<std::uint8_t>& cipher) {
	if (key.size() < DES_KEY_SIZE) {
		return false;
	}
	std::vector<std::uint8_t> desKey(key.begin(), key.begin() + DES_KEY_SIZE);

	if (!GenerateRandomIv(iv)) {
		return false;
	}
	// BCryptEncrypt modifies the IV buffer, so keep a working copy
	std::vector<std::uint8_t> workIv = iv;

	BCRYPT_ALG_HANDLE alg = nullptr;
	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_DES_ALGORITHM, nullptr, 0) != 0) {
		return false;
	}

	bool ok = false;
	BCRYPT_KEY_HANDLE keyHandle = nullptr;
	do {
		if (BCryptSetProperty(alg,
		                      BCRYPT_CHAINING_MODE,
		                      reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
		                      sizeof(BCRYPT_CHAIN_MODE_CBC),
		                      0) != 0) {
			break;
		}
		if (!ImportKey(alg, desKey, keyHandle)) {
			break;
		}

		ULONG required = 0;
		if (BCryptEncrypt(keyHandle,
		                  const_cast<PUCHAR>(plain.data()),
		                  static_cast<ULONG>(plain.size()),
		                  nullptr,
		                  workIv.data(),
		                  static_cast<ULONG>(workIv.size()),
		                  nullptr,
		                  0,
		                  &required,
		                  BCRYPT_BLOCK_PADDING) != 0) {
			break;
		}

		cipher.resize(required);
		if (BCryptEncrypt(keyHandle,
		                  const_cast<PUCHAR>(plain.data()),
		                  static_cast<ULONG>(plain.size()),
		                  nullptr,
		                  workIv.data(),
		                  static_cast<ULONG>(workIv.size()),
		                  cipher.data(),
		                  static_cast<ULONG>(cipher.size()),
		                  &required,
		                  BCRYPT_BLOCK_PADDING) != 0) {
			break;
		}
		cipher.resize(required);
		ok = true;
	} while (false);

	if (keyHandle) {
		BCryptDestroyKey(keyHandle);
	}
	BCryptCloseAlgorithmProvider(alg, 0);
	return ok;
}

bool DesEncryptionHandler::Decrypt(const std::vector<std::uint8_t>& cipher,
                                   const std::vector<std::uint8_t>& key,
                                   const std::vector<std::uint8_t>& iv,
                                   std::vector<std::uint8_t>& plain) {
	if (key.size() < DES_KEY_SIZE || iv.size() != DES_IV_SIZE) {
		return false;
	}
	std::vector<std::uint8_t> desKey(key.begin(), key.begin() + DES_KEY_SIZE);
	// BCryptDecrypt also modifies the IV buffer
	std::vector<std::uint8_t> workIv = iv;

	BCRYPT_ALG_HANDLE alg = nullptr;
	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_DES_ALGORITHM, nullptr, 0) != 0) {
		return false;
	}

	bool ok = false;
	BCRYPT_KEY_HANDLE keyHandle = nullptr;
	do {
		if (BCryptSetProperty(alg,
		                      BCRYPT_CHAINING_MODE,
		                      reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
		                      sizeof(BCRYPT_CHAIN_MODE_CBC),
		                      0) != 0) {
			break;
		}
		if (!ImportKey(alg, desKey, keyHandle)) {
			break;
		}

		ULONG required = 0;
		if (BCryptDecrypt(keyHandle,
		                  const_cast<PUCHAR>(cipher.data()),
		                  static_cast<ULONG>(cipher.size()),
		                  nullptr,
		                  workIv.data(),
		                  static_cast<ULONG>(workIv.size()),
		                  nullptr,
		                  0,
		                  &required,
		                  BCRYPT_BLOCK_PADDING) != 0) {
			break;
		}

		plain.resize(required);
		if (BCryptDecrypt(keyHandle,
		                  const_cast<PUCHAR>(cipher.data()),
		                  static_cast<ULONG>(cipher.size()),
		                  nullptr,
		                  workIv.data(),
		                  static_cast<ULONG>(workIv.size()),
		                  plain.data(),
		                  static_cast<ULONG>(plain.size()),
		                  &required,
		                  BCRYPT_BLOCK_PADDING) != 0) {
			break;
		}
		plain.resize(required);
		ok = true;
	} while (false);

	if (keyHandle) {
		BCryptDestroyKey(keyHandle);
	}
	BCryptCloseAlgorithmProvider(alg, 0);
	return ok;
}