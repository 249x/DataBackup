#pragma once

#include "../Manager.h"
#include "../../General/TypeContainer.h"

#include <cstdint>
#include <filesystem>
#include <memory>
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

    bool SetEncryption(bool enabled, std::uint16_t type);
    bool SetKeyPath(const std::filesystem::path& keyPath);
    bool Enabled() const noexcept;
    std::uint16_t Type() const noexcept;
    bool HasKey() const noexcept;
    bool CheckKey();

private:
    bool enabled = false;
    std::uint16_t type = 0;
    std::vector<std::uint8_t> key;
    std::unordered_map<std::uint16_t, std::unique_ptr<EncryptionHandler>> handlersMap;
};