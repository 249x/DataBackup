#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class EncryptionManager;

class EncryptionHandler {
public:
    EncryptionHandler(EncryptionManager& manager);
    virtual ~EncryptionHandler();

    virtual std::uint16_t GetAlgorithmID() const = 0;

    // 显示名（如 "AES"），由管理器汇总后交给前端，界面不自己写算法清单
    virtual const char* Name() const = 0;

    // 用途说明，界面直接拼进提示文字。基类给一句通用的，子类想更准确再覆写
    virtual const char* Description() const { return "加密算法"; }

    // 密钥短于该长度时 Encrypt 必然失败，调用方据此提前报错
    virtual std::size_t MinimumKeySize() const = 0;

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
