#include "EncryptionManager.h"

#include "Handler/EncryptionHandler.h"
#include "Handler/AesEncryptionHandler.h"
#include "Handler/DesEncryptionHandler.h"
#include "../Command/CommandManager.h"
#include "../FileIO/FileIOManager.h"
#include "../../General/Debug.h"

#include <memory>

EncryptionManager::EncryptionManager(System& sys) : Manager(sys) {}

EncryptionManager::~EncryptionManager() {}

void EncryptionManager::Initialize() {
	handlersMap[AesEncryptionHandler::AlgorithmID] =
		std::make_unique<AesEncryptionHandler>(*this);
	handlersMap[DesEncryptionHandler::AlgorithmID] =
		std::make_unique<DesEncryptionHandler>(*this);


	Manager::Get<CommandManager>()->RegisterCommand(
		"set-encrypt", "Set encryption: <enable> <algorithm>", SetEncryption, this);
	Manager::Get<CommandManager>()->RegisterCommand("key", "Set path of key", SetKeyPath, this);
}

bool EncryptionManager::SetKeyPath(const std::filesystem::path& keyPath) {
	FileIOManager* io = Manager::Get<FileIOManager>();
	if (io == nullptr || !io->ReadContent(keyPath, key)) {
		Debug::Error("Failed to read key file: " + keyPath.string(), "Encryption");
		return false;
	}
	return true;
}

bool EncryptionManager::HasKey() const noexcept {
	return !key.empty();
}


bool EncryptionManager::CheckKey() {
	EncryptionHandler* handler = Get(type);
	if (handler == nullptr) {
		Debug::Error("No encryption handler with id " + std::to_string(type), "Encryption");
		return false;
	}
	if (key.empty()) {
		Debug::Error("No key is loaded, run 'key <path>' first", "Encryption");
		return false;
	}
	if (key.size() < handler->MinimumKeySize()) {
		Debug::Error("Key is too short for algorithm " + std::to_string(type) + ": need " +
		                 std::to_string(handler->MinimumKeySize()) + " bytes, got " +
		                 std::to_string(key.size()),
		             "Encryption");
		return false;
	}
	return true;
}

bool EncryptionManager::SetEncryption(bool enabled, std::uint16_t type) {
	if (enabled && Get(type) == nullptr) {
		Debug::Error("No encryption handler with id " + std::to_string(type), "Encryption");
		return false;
	}
	this->enabled = enabled;
	this->type = type;
	return true;
}

bool EncryptionManager::Enabled() const noexcept {
	return enabled;
}

std::uint16_t EncryptionManager::Type() const noexcept {
	return type;
}

EncryptionHandler* EncryptionManager::Get(std::uint16_t type) const {
	auto it = handlersMap.find(type);
	if (it == handlersMap.end()) {
		return nullptr;
	}
	return it->second.get();
}

bool EncryptionManager::Encryption(const std::vector<std::uint8_t>& data,
	std::uint16_t type,
	std::vector<std::uint8_t>& output)
{
	auto it = handlersMap.find(type);
	if (it == handlersMap.end() || !it->second) {
		Debug::Error("No encryption handler with id " + std::to_string(type), "Encryption");
		return false;
	}
	if (!it->second->Encrypt(data, key, output)) {
		Debug::Error("Encrypt failed", "Encryption");
		return false;
	}
	return true;
}

bool EncryptionManager::Decryption(const std::vector<std::uint8_t>& data,
	std::uint16_t type,
	std::vector<std::uint8_t>& output)
{
	auto it = handlersMap.find(type);
	if (it == handlersMap.end() || !it->second) {
		Debug::Error("No encryption handler with id " + std::to_string(type), "Encryption");
		return false;
	}
	if (!it->second->Decrypt(data, key, output)) {
		Debug::Error("Decrypt failed", "Encryption");
		return false;
	}
	return true;
}