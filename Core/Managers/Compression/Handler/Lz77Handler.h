#pragma once

#include "CompressionHandler.h"
#include <cstdint>
#include <vector>

/*
 * LZ77 压缩算法。
 * 格式：[原始长度 8字节小端] + [压缩流]
 * 压缩流由字面量字节和回溯引用构成。
 */
class Lz77Handler : public CompressionHandler
{
public:
    Lz77Handler();
    ~Lz77Handler() override;

    bool Compression(const std::vector<uint8_t>& input,
                     std::vector<uint8_t>& output) override;

    bool Decompression(const std::vector<uint8_t>& input,
                       std::vector<uint8_t>& output) override;

    std::uint16_t GetID() const override;
    const char* Name() const override;
    const char* Description() const override;

private:
    static constexpr size_t WindowSize = 4096; // 滑动窗口大小
    static constexpr size_t MinMatch = 3;      // 最小匹配长度
    static constexpr size_t MaxMatch = 258;    // 最大匹配长度
};