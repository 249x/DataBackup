#include "AesEncryptionHandler.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstring>

#pragma comment(lib, "bcrypt.lib")

namespace {
	constexpr std::size_t AES_KEY_SIZE = 32;
	constexpr std::size_t AES_IV_SIZE = 16;

	bool GenerateRandomIv(std::vector<std::uint8_t>& iv) {
		iv.resize(AES_IV_SIZE);
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

AesEncryptionHandler::AesEncryptionHandler(EncryptionManager& manager)
	: EncryptionHandler(manager) {}

AesEncryptionHandler::~AesEncryptionHandler() = default;

std::uint16_t AesEncryptionHandler::GetAlgorithmID() const {
	return AlgorithmID;
}

bool AesEncryptionHandler::Encrypt(const std::vector<std::uint8_t>& input,
                                   const std::vector<std::uint8_t>& key,
                                   std::vector<std::uint8_t>& output) {
	if (key.size() < AES_KEY_SIZE) {
		return false;
	}
	std::vector<std::uint8_t> aesKey(key.begin(), key.begin() + AES_KEY_SIZE);

	// 1. 生成随机 IV
	std::vector<std::uint8_t> iv;
	if (!GenerateRandomIv(iv)) {
		return false;
	}
	std::vector<std::uint8_t> workIv = iv;

	// 2. 加密
	BCRYPT_ALG_HANDLE alg = nullptr;
	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) {
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
		if (!ImportKey(alg, aesKey, keyHandle)) {
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

	// 3. 输出 [IV][密文]
	output.clear();
	output.reserve(iv.size() + cipher.size());
	output.insert(output.end(), iv.begin(), iv.end());
	output.insert(output.end(), cipher.begin(), cipher.end());
	return true;
}

bool AesEncryptionHandler::Decrypt(const std::vector<std::uint8_t>& input,
                                   const std::vector<std::uint8_t>& key,
                                   std::vector<std::uint8_t>& output) {
	if (key.size() < AES_KEY_SIZE || input.size() < AES_IV_SIZE) {
		return false;
	}
	std::vector<std::uint8_t> aesKey(key.begin(), key.begin() + AES_KEY_SIZE);

	// 1. 从 input 开头读 IV
	std::vector<std::uint8_t> workIv(input.begin(), input.begin() + AES_IV_SIZE);

	// 2. 密文从 IV 之后开始
	std::vector<std::uint8_t> cipher(input.begin() + AES_IV_SIZE, input.end());

	// 3. 解密
	BCRYPT_ALG_HANDLE alg = nullptr;
	if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) {
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
		if (!ImportKey(alg, aesKey, keyHandle)) {
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