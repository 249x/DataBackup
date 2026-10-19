#include "Lz77Handler.h"
#include <cstring>
#include <algorithm>

Lz77Handler::Lz77Handler() = default;
Lz77Handler::~Lz77Handler() = default;

std::uint16_t Lz77Handler::GetID() const {
    return 3;
}

const char* Lz77Handler::Name() const {
    return "LZ77";
}

const char* Lz77Handler::Description() const {
    return "适合重复文本";
}

bool Lz77Handler::Compression(const std::vector<uint8_t>& input,
                              std::vector<uint8_t>& output)
{
    output.clear();
    
    // 1. 头部：原始长度 8 字节小端
    uint64_t originalSize = input.size();
    for (int i = 0; i < 8; ++i) {
        output.push_back(static_cast<uint8_t>((originalSize >> (8 * i)) & 0xFF));
    }

    if (input.empty()) {
        return true;
    }

    size_t i = 0;
    const size_t n = input.size();

    while (i < n) {
        size_t bestLen = 0;
        size_t bestDist = 0;

        // 2. 在滑动窗口中寻找最长匹配
        size_t searchStart = (i > WindowSize) ? (i - WindowSize) : 0;
        size_t maxLookahead = std::min(n - i, MaxMatch);

        for (size_t j = searchStart; j < i; ++j) {
            size_t len = 0;
            // 贪心向后匹配
            while (len < maxLookahead && input[j + len] == input[i + len]) {
                ++len;
            }
            if (len > bestLen) {
                bestLen = len;
                bestDist = i - j;
                if (bestLen == MaxMatch) break; // 达到最大长度，停止搜索
            }
        }

        // 3. 如果找到足够长的匹配，输出回溯标记
        if (bestLen >= MinMatch) {
            // 格式：[0xFF] [距离 2字节小端] [长度 1字节]
            output.push_back(0xFF);
            output.push_back(static_cast<uint8_t>(bestDist & 0xFF));
            output.push_back(static_cast<uint8_t>((bestDist >> 8) & 0xFF));
            output.push_back(static_cast<uint8_t>(bestLen));
            i += bestLen;
        } else {
            // 4. 否则输出字面量
            uint8_t b = input[i];
            output.push_back(b);
            // 字面量 0xFF 会和回溯标记撞车，用 4 字节的 FF 00 00 00 转义
            // （回溯标记里距离至少是 1，所以这串不可能是合法匹配）
            if (b == 0xFF) {
                output.push_back(0x00);
                output.push_back(0x00);
                output.push_back(0x00);
            }
            ++i;
        }
    }

    return true;
}

bool Lz77Handler::Decompression(const std::vector<uint8_t>& input,
                                std::vector<uint8_t>& output)
{
    output.clear();

    if (input.size() < 8) return false;

    // 1. 读原始长度
    uint64_t originalSize = 0;
    for (int i = 0; i < 8; ++i) {
        originalSize |= static_cast<uint64_t>(input[i]) << (8 * i);
    }

    if (originalSize == 0) return true;

    output.reserve(static_cast<size_t>(originalSize));

    size_t pos = 8;
    while (pos < input.size() && output.size() < originalSize) {
        uint8_t b = input[pos];

        if (b != 0xFF) {
            // 普通字面量
            output.push_back(b);
            ++pos;
            continue;
        }

        // 0xFF 打头一定是 4 字节记号：后面 3 个 0 是转义的字面量，否则是回溯标记
        if (pos + 3 >= input.size()) {
            return false; // 记号不完整
        }
        const uint8_t b1 = input[pos + 1];
        const uint8_t b2 = input[pos + 2];
        const uint8_t b3 = input[pos + 3];

        if (b1 == 0 && b2 == 0 && b3 == 0) {
            output.push_back(0xFF); // 转义的字面量
            pos += 4;
            continue;
        }

        const std::uint16_t dist = static_cast<std::uint16_t>(b1 | (static_cast<std::uint16_t>(b2) << 8));
        if (dist == 0 || dist > output.size() || b3 < MinMatch) {
            return false; // 数据坏了
        }

        // 复制匹配（注意重叠复制的情况，比如距离为 1 的重复）
        for (std::size_t k = 0; k < static_cast<std::size_t>(b3) && output.size() < originalSize;
             ++k) {
            output.push_back(output[output.size() - dist]);
        }
        pos += 4;
    }

    return output.size() == originalSize;
}