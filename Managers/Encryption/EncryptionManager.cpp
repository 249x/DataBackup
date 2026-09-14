#include "EncryptionManager.h"

#include "EncryptionHeader.h"
#include "Handler/EncryptionHandler.h"
#include "Handler/AesEncryptionHandler.h"
#include "Handler/DesEncryptionHandler.h"

#include <memory>
#include <utility>

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

	std::vector<std::uint8_t> additionalInfo;   // 输出 IV
	std::vector<std::uint8_t> cipher;
	if (!it->second->Process(data, key, additionalInfo, cipher)) {
		return false;
	}

	EncryptionHeader header;
	header.SetAlgorithm(type);
	header.SetAlgorithmInfo(additionalInfo);
	std::vector<std::uint8_t> headerBytes = header.Serialize();
	if (headerBytes.empty() && !additionalInfo.empty()) {
		return false;
	}

	output.clear();
	output.reserve(headerBytes.size() + cipher.size());
	output.insert(output.end(), headerBytes.begin(), headerBytes.end());
	output.insert(output.end(), cipher.begin(), cipher.end());
	return true;
}

bool EncryptionManager::Decryption(const std::vector<std::uint8_t>& data,
	const std::vector<std::uint8_t>& key,
	std::vector<std::uint8_t>& output)
{
	EncryptionHeader header;
	if (!header.Deserialize(data)) {
		return false;
	}

	auto it = handlersMap.find(header.Algorithm());
	if (it == handlersMap.end() || !it->second) {
		return false;
	}

	const std::size_t headerSize = header.HeaderSize();
	if (headerSize > data.size()) {
		return false;
	}

	std::vector<std::uint8_t> cipher(data.begin() + headerSize, data.end());
	std::vector<std::uint8_t> additionalInfo = header.AlgorithmInfo();   // 输入 IV
	return it->second->Process(cipher, key, additionalInfo, output);
}