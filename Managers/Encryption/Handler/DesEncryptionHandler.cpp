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
			nullptr, iv.data(), static_cast<ULONG>(iv.size()),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG);
		return status == 0;
	}

	bool ImportKey(BCRYPT_ALG_HANDLE alg,
	               const std::vector<std::uint8_t>& key,
	               BCRYPT_KEY_HANDLE& keyHandle) {
		return BCryptGenerateSymmetricKey(
			alg, &keyHandle, nullptr, 0,
			const_cast<PUCHAR>(key.data()),
			static_cast<ULONG>(key.size()), 0) == 0;
	}
}

DesEncryptionHandler::DesEncryptionHandler(EncryptionManager& manager)
	: EncryptionHandler(manager) {}

DesEncryptionHandler::~DesEncryptionHandler() = default;

std::uint16_t DesEncryptionHandler::GetAlgorithmID() const {
	return AlgorithmID;
}

bool DesEncryptionHandler::Encrypt(const std::vector<std::uint8_t>& input,
                                   const std::vector<std::uint8_t>& key,
                                   std::vector<std::uint8_t>& output) {
	if (key.size() < DES_KEY_SIZE) {
		return false;
	}
	std::vector<std::uint8_t> desKey(key.begin(), key.begin() + DES_KEY_SIZE);

	std::vector<std::uint8_t> iv;
	if (!GenerateRandomIv(iv)) {
		return false;
	}
	std::vector<std::uint8_t> workIv = iv;

	BCRYPT_ALG_HANDLE alg = nullptr;
	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_DES_ALGORITHM, nullptr, 0) != 0) {
		return false;
	}

	bool ok = false;
	BCRYPT_KEY_HANDLE keyHandle = nullptr;
	std::vector<std::uint8_t> cipher;
	do {
		if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
		                      reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
		                      sizeof(BCRYPT_CHAIN_MODE_CBC), 0) != 0) {
			break;
		}
		if (!ImportKey(alg, desKey, keyHandle)) {
			break;
		}

		ULONG required = 0;
		if (BCryptEncrypt(keyHandle, const_cast<PUCHAR>(input.data()),
		                  static_cast<ULONG>(input.size()), nullptr,
		                  workIv.data(), static_cast<ULONG>(workIv.size()),
		                  nullptr, 0, &required, BCRYPT_BLOCK_PADDING) != 0) {
			break;
		}

		cipher.resize(required);
		if (BCryptEncrypt(keyHandle, const_cast<PUCHAR>(input.data()),
		                  static_cast<ULONG>(input.size()), nullptr,
		                  workIv.data(), static_cast<ULONG>(workIv.size()),
		                  cipher.data(), static_cast<ULONG>(cipher.size()),
		                  &required, BCRYPT_BLOCK_PADDING) != 0) {
			break;
		}
		cipher.resize(required);
		ok = true;
	} while (false);

	if (keyHandle) BCryptDestroyKey(keyHandle);
	BCryptCloseAlgorithmProvider(alg, 0);

	if (!ok) return false;

	output.clear();
	output.reserve(iv.size() + cipher.size());
	output.insert(output.end(), iv.begin(), iv.end());
	output.insert(output.end(), cipher.begin(), cipher.end());
	return true;
}

bool DesEncryptionHandler::Decrypt(const std::vector<std::uint8_t>& input,
                                   const std::vector<std::uint8_t>& key,
                                   std::vector<std::uint8_t>& output) {
	if (key.size() < DES_KEY_SIZE || input.size() < DES_IV_SIZE) {
		return false;
	}
	std::vector<std::uint8_t> desKey(key.begin(), key.begin() + DES_KEY_SIZE);

	std::vector<std::uint8_t> workIv(input.begin(), input.begin() + DES_IV_SIZE);
	std::vector<std::uint8_t> cipher(input.begin() + DES_IV_SIZE, input.end());

	BCRYPT_ALG_HANDLE alg = nullptr;
	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_DES_ALGORITHM, nullptr, 0) != 0) {
		return false;
	}

	bool ok = false;
	BCRYPT_KEY_HANDLE keyHandle = nullptr;
	do {
		if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
		                      reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
		                      sizeof(BCRYPT_CHAIN_MODE_CBC), 0) != 0) {
			break;
		}
		if (!ImportKey(alg, desKey, keyHandle)) {
			break;
		}

		ULONG required = 0;
		if (BCryptDecrypt(keyHandle, const_cast<PUCHAR>(cipher.data()),
		                  static_cast<ULONG>(cipher.size()), nullptr,
		                  workIv.data(), static_cast<ULONG>(workIv.size()),
		                  nullptr, 0, &required, BCRYPT_BLOCK_PADDING) != 0) {
			break;
		}

		output.resize(required);
		if (BCryptDecrypt(keyHandle, const_cast<PUCHAR>(cipher.data()),
		                  static_cast<ULONG>(cipher.size()), nullptr,
		                  workIv.data(), static_cast<ULONG>(workIv.size()),
		                  output.data(), static_cast<ULONG>(output.size()),
		                  &required, BCRYPT_BLOCK_PADDING) != 0) {
			break;
		}
		output.resize(required);
		ok = true;
	} while (false);

	if (keyHandle) BCryptDestroyKey(keyHandle);
	BCryptCloseAlgorithmProvider(alg, 0);
	return ok;
}