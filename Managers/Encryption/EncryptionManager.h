#pragma once

#include "../Manager.h"
#include "../../General/TypeContainer.h"
#include "../../FileStruct/StructuredFileContent.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

class EncryptionHandler;

class EncryptionManager : public Manager {
public:
	EncryptionManager(System& sys);
	~EncryptionManager() override;

	void Initialize() override;

	bool Encryption(const std::vector<std::uint8_t>& data, 
		const std::uint16_t& type, 
		const std::vector<std::uint8_t>& key, 
		std::vector<std::uint8_t>& output);

	//类型和额外信息从文件内容头读
	bool Decryption(const std::vector<std::uint8_t>& data,
		const std::uint16_t& type, 
		const std::vector<std::uint8_t>& key,
		std::vector<std::uint8_t>& output);


private:
	std::unordered_map<std::uint16_t, std::unique_ptr<EncryptionHandler>> handlersMap;
};