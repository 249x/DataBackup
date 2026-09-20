#pragma once

#include "CompressionHandler.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>

class HuffmanHandler : public CompressionHandler
{
public:
    HuffmanHandler();
    ~HuffmanHandler() override;

    bool Compression(const std::vector<uint8_t>& input,
                     std::vector<uint8_t>& output) override;

    bool Decompression(const std::vector<uint8_t>& input,
                       std::vector<uint8_t>& output) override;

    std::uint16_t GetID() const override;

private:
    struct Node
    {
        uint8_t  byte = 0;
        uint64_t freq = 0;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        bool IsLeaf() const { return !left && !right; }
    };

    // 构建霍夫曼树，freq 输出频率表
    std::unique_ptr<Node> BuildTree(const std::vector<uint8_t>& input,
                                    std::unordered_map<uint8_t, uint64_t>& freq) const;

    // 递归生成编码表
    void BuildCodeTable(const Node* node,
                        const std::vector<bool>& path,
                        std::unordered_map<uint8_t, std::vector<bool>>& table) const;

    // 序列化 / 反序列化霍夫曼树
    void SerializeTree(const Node* node, std::vector<uint8_t>& out) const;
    std::unique_ptr<Node> DeserializeTree(const std::vector<uint8_t>& in,
                                          size_t& pos) const;

    // 位读写
    static void WriteBit(std::vector<uint8_t>& out,
                         uint8_t& cur, int& bitCount, bool bit);
    static bool ReadBit(const std::vector<uint8_t>& in,
                        size_t& bytePos, int& bitPos);
};