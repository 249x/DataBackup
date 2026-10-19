#pragma once

#include "CompressionHandler.h"
#include <cstdint>
#include <vector>

/*
 * LZ77 压缩算法。
 *
 * 格式：[原始长度 8 字节小端] + [压缩流]
 * 压缩流是一串记号，每个记号看首字节就能分辨：
 *   首字节 != 0xFF   → 字面量（就是这个字节）
 *   FF 00 00 00      → 转义的字面量 0xFF（4 字节，与回溯标记等宽）
 *   FF dlo dhi len   → 回溯标记：距离 = dlo | (dhi << 8)（至少 1），长度 = len（3..255）
 *
 * 长度只占 1 字节，所以最大匹配长度是 **255**（不是 258——那样 258 会被截成 2）。
 * 字面量 0xFF 为什么不用 2 字节的 FF 00 转义：距离为 256 / 512 / … 的匹配本身就是
 * FF 00 01 xx，两者会撞车。回溯标记的距离至少是 1，所以 FF 00 00 xx 不可能是合法匹配。
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
    static constexpr size_t MaxMatch = 255;    // 最大匹配长度（长度字段只有 1 字节）
};