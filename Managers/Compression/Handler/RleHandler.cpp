#include "RleHandler.h"

// ============ 构造 / 析构 ============
RleHandler::RleHandler() = default;
RleHandler::~RleHandler() = default;

// ============ ID ============
std::uint16_t RleHandler::GetID() const{
    return 2;
}

// ============ 压缩 ============
//
// 格式：
//   [原始长度 8 字节小端]
//   [RLE 数据流]
//
// RLE 数据流由若干"块"组成，每块 2 字节：
//   字节 0: count（1..255）
//   字节 1: value
// 表示 value 重复 count 次。
//
// 连续相同字节超过 255 个时，拆成多个块。
bool RleHandler::Compression(const std::vector<uint8_t>& input,
                             std::vector<uint8_t>& output){
    output.clear();

    // 头部：原始长度 8 字节小端
    uint64_t originalSize = input.size();
    for (int i = 0; i < 8; ++i)
        output.push_back(static_cast<uint8_t>((originalSize >> (8 * i)) & 0xFF));

    if (input.empty())
        return true;

    size_t i = 0;
    const size_t n = input.size();
    while (i < n)
    {
        uint8_t value = input[i];
        uint8_t count = 1;
        ++i;
        // 向后数相同字节，最多 255 个
        while (i < n && input[i] == value && count < 255)
        {
            ++count;
            ++i;
        }
        output.push_back(count);
        output.push_back(value);
    }

    return true;
}

// ============ 解压 ============
bool RleHandler::Decompression(const std::vector<uint8_t>& input,
                               std::vector<uint8_t>& output){
    output.clear();

    if (input.size() < 8) return false;

    // 1. 读原始长度
    uint64_t originalSize = 0;
    for (int i = 0; i < 8; ++i)
        originalSize |= static_cast<uint64_t>(input[i]) << (8 * i);

    if (originalSize == 0)
        return true;

    // 2. 解析 RLE 块
    output.reserve(static_cast<size_t>(originalSize));

    size_t pos = 8;
    while (pos + 1 < input.size() && output.size() < originalSize)
    {
        uint8_t count = input[pos++];
        uint8_t value = input[pos++];

        if (count == 0)
            return false;   // 非法块

        for (uint8_t k = 0; k < count && output.size() < originalSize; ++k)
            output.push_back(value);
    }

    return output.size() == originalSize;
}