#pragma once

#include "EncryptionHandler.h"

class DesEncryptionHandler : public EncryptionHandler {
public:
	static constexpr std::uint16_t AlgorithmID = 2;

	DesEncryptionHandler(EncryptionManager& manager);
	~DesEncryptionHandler() override;

	std::uint16_t GetAlgorithmID() const override;
	std::size_t MinimumKeySize() const override;

	bool Encrypt(const std::vector<std::uint8_t>& input,
	             const std::vector<std::uint8_t>& key,
	             std::vector<std::uint8_t>& output) override;

	bool Decrypt(const std::vector<std::uint8_t>& input,
	             const std::vector<std::uint8_t>& key,
	             std::vector<std::uint8_t>& output) override;
};