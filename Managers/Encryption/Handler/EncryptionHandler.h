#pragma once

#include "../EncryptionManager.h"

#include <cstdint>
#include <vector>

class EncryptionHandler {
public:
	EncryptionHandler(EncryptionManager& manager);
	virtual ~EncryptionHandler();

	virtual std::uint16_t GetAlgorithmID() const = 0;

	virtual bool Process(const std::vector<std::uint8_t>& input,
	                    const std::vector<std::uint8_t>& additionalInfo,
	                    std::vector<std::uint8_t>& output) = 0;

	template <typename T>
	T* Get() {
		return manager->Get<T>();
	}

private:
	EncryptionManager* manager;
};