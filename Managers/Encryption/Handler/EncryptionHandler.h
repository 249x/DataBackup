#pragma once

#include <cstdint>
#include <vector>

class EncryptionManager;

class EncryptionHandler {
public:
    EncryptionHandler(EncryptionManager& manager);
    virtual ~EncryptionHandler();

    virtual std::uint16_t GetAlgorithmID() const = 0;

    // ??: input=??, key=??, output=[IV][??]
    virtual bool Encrypt(const std::vector<std::uint8_t>& input,
                         const std::vector<std::uint8_t>& key,
                         std::vector<std::uint8_t>& output) = 0;

    // ??: input=[IV][??], key=??, output=??
    virtual bool Decrypt(const std::vector<std::uint8_t>& input,
                         const std::vector<std::uint8_t>& key,
                         std::vector<std::uint8_t>& output) = 0;

private:
    EncryptionManager* manager;
};
