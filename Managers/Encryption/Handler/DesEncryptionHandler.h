#pragma once

#include "EncryptionHandler.h"

class DesEncryptionHandler : public EncryptionHandler {
public:
	static constexpr std::uint16_t AlgorithmID = 2;

	DesEncryptionHandler(EncryptionManager& manager);
	~DesEncryptionHandler() override;

	std::uint16_t GetAlgorithmID() const override;

	bool Encrypt(const std::vector<std::uint8_t>& input,
	             const std::vector<std::uint8_t>& key,
	             std::vector<std::uint8_t>& output) override;

	bool Decrypt(const std::vector<std::uint8_t>& input,
	             const std::vector<std::uint8_t>& key,
	             std::vector<std::uint8_t>& output) override;

private:
	static constexpr std::size_t KeySize = 8;
	static constexpr std::size_t IvSize = 8;
};