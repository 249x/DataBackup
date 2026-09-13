#include "EncryptionManager.h"

#include "Handler/EncryptionHandler.h"

EncryptionManager::EncryptionManager(System& sys) : Manager(sys) {}

EncryptionManager::~EncryptionManager() {}

void EncryptionManager::Initialize() {}

bool EncryptionManager::Encryption(const std::vector<std::uint8_t>& data, 
	const std::uint16_t& type, 
	const std::vector<std::uint8_t>& key, 
	std::vector<std::uint8_t>& output)
{


    //to do
    return true;
}

bool EncryptionManager::Decryption(const std::vector<std::uint8_t>& data,
	const std::uint16_t& type, 
	const std::vector<std::uint8_t>& key,
	std::vector<std::uint8_t>& output)
{


    //to do
    return true;
}