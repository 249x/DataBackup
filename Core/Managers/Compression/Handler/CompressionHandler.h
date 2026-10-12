#pragma once

#include <cstdint>
#include <vector>


class CompressionHandler
{
public:
    CompressionHandler();
    virtual ~CompressionHandler();

    virtual bool Compression(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) = 0;
    virtual bool Decompression(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) = 0;
    virtual std::uint16_t GetID() const = 0;
    // 显示名（如 "Huffman"），由管理器汇总后交给前端，界面不自己写算法清单
    virtual const char* Name() const = 0;
    // 用途说明，界面直接拼进提示文字。基类给一句通用的，子类想更准确再覆写
    virtual const char* Description() const { return "压缩算法"; }

};

