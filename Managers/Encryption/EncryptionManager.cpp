#include "EncryptionManager.h"

#include "Handler/EncryptionHandler.h"
#include "Handler/AesEncryptionHandler.h"
#include "Handler/DesEncryptionHandler.h"

#include <memory>

EncryptionManager::EncryptionManager(System& sys) : Manager(sys) {}

EncryptionManager::~EncryptionManager() {}

void EncryptionManager::Initialize() {
	handlersMap[AesEncryptionHandler::AlgorithmID] =
		std::make_unique<AesEncryptionHandler>(*this);
	handlersMap[DesEncryptionHandler::AlgorithmID] =
		std::make_unique<DesEncryptionHandler>(*this);
}

bool EncryptionManager::Encryption(const std::vector<std::uint8_t>& data,
	const std::uint16_t& type,
	const std::vector<std::uint8_t>& key,
	std::vector<std::uint8_t>& output)
{
	auto it = handlersMap.find(type);
	if (it == handlersMap.end() || !it->second) {
		return false;
	}
	return it->second->Encrypt(data, key, output);
}

bool EncryptionManager::Decryption(const std::vector<std::uint8_t>& data,
	const std::uint16_t& type,
	const std::vector<std::uint8_t>& key,
	std::vector<std::uint8_t>& output)
{
	auto it = handlersMap.find(type);
	if (it == handlersMap.end() || !it->second) {
		return false;
	}
	return it->second->Decrypt(data, key, output);
}