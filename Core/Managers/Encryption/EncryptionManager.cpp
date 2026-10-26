#include "EncryptionManager.h"

#include "Handler/EncryptionHandler.h"
#include "Handler/AesEncryptionHandler.h"
#include "Handler/DesEncryptionHandler.h"
#include "../Command/CommandManager.h"
#include "../FileIO/FileIOManager.h"
#include "../../General/Debug.h"
#include "../../General/Utf8.h"

#include <algorithm>
#include <memory>

namespace {

// 失败原因里最有用的三项：哪个算法、数据多大、钥匙多长（够不够）
std::string Digest(const EncryptionHandler& handler, std::uint16_t type, std::size_t inputSize,
                   std::size_t keySize) {
	return std::string(handler.Name()) + " (id " + std::to_string(type) + "), input " +
	       std::to_string(inputSize) + " bytes, key " + std::to_string(keySize) +
	       " bytes (needs at least " + std::to_string(handler.MinimumKeySize()) + ")";
}

} // namespace

EncryptionManager::EncryptionManager(System& sys) : Manager(sys) {}

EncryptionManager::~EncryptionManager() {}

void EncryptionManager::Initialize() {
	handlersMap[AesEncryptionHandler::AlgorithmID] =
		std::make_unique<AesEncryptionHandler>(*this);
	handlersMap[DesEncryptionHandler::AlgorithmID] =
		std::make_unique<DesEncryptionHandler>(*this);


	Manager::Get<CommandManager>()->RegisterCommand("key", "Set path of key", SetKeyPath, this);
}

bool EncryptionManager::SetKeyPath(const std::filesystem::path& keyPath) {
	FileIOManager* io = Manager::Get<FileIOManager>();
	if (io == nullptr || !io->ReadContent(keyPath, key)) {
		Debug::Error("Failed to read key file: " + Utf8::FromPath(keyPath), LogInfo);
		return false;
	}
	return true;
}

bool EncryptionManager::HasKey() const noexcept {
	return !key.empty();
}


bool EncryptionManager::CheckKey(std::uint16_t type) {
	EncryptionHandler* handler = Get(type);
	if (handler == nullptr) {
		Debug::Error("No encryption handler with id " + std::to_string(type), LogInfo);
		return false;
	}
	if (key.empty()) {
		Debug::Error("No key is loaded, run 'key <path>' first", LogInfo);
		return false;
	}
	if (key.size() < handler->MinimumKeySize()) {
		Debug::Error("Key is too short for algorithm " + std::to_string(type) + ": need " +
		                 std::to_string(handler->MinimumKeySize()) + " bytes, got " +
		                 std::to_string(key.size()),
		             LogInfo);
		return false;
	}
	return true;
}

EncryptionHandler* EncryptionManager::Get(std::uint16_t type) const {
	auto it = handlersMap.find(type);
	if (it == handlersMap.end()) {
		return nullptr;
	}
	return it->second.get();
}

std::vector<AlgorithmInfo> EncryptionManager::Algorithms() const {
	std::vector<AlgorithmInfo> list;
	list.reserve(handlersMap.size());
	for (const auto& pair : handlersMap) {
		if (pair.second) {
			list.push_back(AlgorithmInfo{pair.first, pair.second->Name(),
			                             pair.second->Description()});
		}
	}
	// 保证稳定
	std::sort(list.begin(), list.end(),
	          [](const AlgorithmInfo& left, const AlgorithmInfo& right) {
		          return left.id < right.id;
	          });
	return list;
}

std::uint16_t EncryptionManager::IdOf(const std::string& name) const noexcept {
	if (name.empty()) {
		return 0;
	}
	for (const auto& pair : handlersMap) {
		if (pair.second && name == pair.second->Name()) {
			return pair.first;
		}
	}
	return 0;
}

std::size_t EncryptionManager::KeySizeOf(std::uint16_t type) const noexcept {
	EncryptionHandler* handler = Get(type);
	return handler != nullptr ? handler->MinimumKeySize() : 0;
}

bool EncryptionManager::Encryption(const std::vector<std::uint8_t>& data,
	std::uint16_t type,
	std::vector<std::uint8_t>& output)
{
	auto it = handlersMap.find(type);
	if (it == handlersMap.end() || !it->second) {
		Debug::Error("No encryption handler with id " + std::to_string(type), LogInfo);
		return false;
	}
	if (!it->second->Encrypt(data, key, output)) {
		Debug::Error("Encrypt failed: " + Digest(*it->second, type, data.size(), key.size()),
		             LogInfo);
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
		Debug::Error("No encryption handler with id " + std::to_string(type), LogInfo);
		return false;
	}
	if (!it->second->Decrypt(data, key, output)) {
		Debug::Error("Decrypt failed: " + Digest(*it->second, type, data.size(), key.size()) +
		                 ", wrong key or corrupted data?",
		             LogInfo);
		return false;
	}
	return true;
}