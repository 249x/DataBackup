#pragma once

#include "EncryptionHandler.h"

class AesEncryptionHandler : public EncryptionHandler {
public:
	static constexpr std::uint16_t AlgorithmID = 1;

	AesEncryptionHandler(EncryptionManager& manager);
	~AesEncryptionHandler() override;

	std::uint16_t GetAlgorithmID() const override;

	bool Encrypt(const std::vector<std::uint8_t>& input,
	             const std::vector<std::uint8_t>& key,
	             std::vector<std::uint8_t>& output) override;

	bool Decrypt(const std::vector<std::uint8_t>& input,
	             const std::vector<std::uint8_t>& key,
	             std::vector<std::uint8_t>& output) override;

private:
	static constexpr std::size_t KeySize = 32;
	static constexpr std::size_t IvSize = 16;
};