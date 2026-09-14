#pragma once

#include "EncryptionHandler.h"

class AesEncryptionHandler : public EncryptionHandler {
public:
	static constexpr std::uint16_t AlgorithmID = 1;

	AesEncryptionHandler(EncryptionManager& manager);
	~AesEncryptionHandler() override;

	std::uint16_t GetAlgorithmID() const override;

	bool Process(const std::vector<std::uint8_t>& input,
	             const std::vector<std::uint8_t>& key,
	             std::vector<std::uint8_t>& additionalInfo,
	             std::vector<std::uint8_t>& output) override;

private:
	static constexpr std::size_t KeySize = 32;    // AES-256
	static constexpr std::size_t IvSize = 16;
	static constexpr std::size_t BlockSize = 16;

	bool Encrypt(const std::vector<std::uint8_t>& plain,
	             const std::vector<std::uint8_t>& key,
	             std::vector<std::uint8_t>& iv,
	             std::vector<std::uint8_t>& cipher);

	bool Decrypt(const std::vector<std::uint8_t>& cipher,
	             const std::vector<std::uint8_t>& key,
	             const std::vector<std::uint8_t>& iv,
	             std::vector<std::uint8_t>& plain);
};