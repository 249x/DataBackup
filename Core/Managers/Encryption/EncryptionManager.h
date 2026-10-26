#pragma once

#include "../AlgorithmInfo.h"
#include "../Manager.h"
#include "../../General/TypeContainer.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class EncryptionHandler;

class EncryptionManager : public Manager {
public:
    EncryptionManager(System& sys);
    ~EncryptionManager() override;

    void Initialize() override;

    bool Encryption(const std::vector<std::uint8_t>& data, std::uint16_t type,
        std::vector<std::uint8_t>& output);
    bool Decryption(const std::vector<std::uint8_t>& data, std::uint16_t type,
        std::vector<std::uint8_t>& output);

    EncryptionHandler* Get(std::uint16_t type) const;

    std::vector<AlgorithmInfo> Algorithms() const;
    std::uint16_t IdOf(const std::string& name) const noexcept;
    std::size_t KeySizeOf(std::uint16_t type) const noexcept;

    bool SetKeyPath(const std::filesystem::path& keyPath);
    bool HasKey() const noexcept;
    bool CheckKey(std::uint16_t type);

private:
	static constexpr const char* LogInfo = "Encryption";
    std::vector<std::uint8_t> key;
    std::unordered_map<std::uint16_t, std::unique_ptr<EncryptionHandler>> handlersMap;
};