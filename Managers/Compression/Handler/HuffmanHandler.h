#pragma once

#include "CompressionHandler.h"

#include <array>
#include <memory>
#include <queue>

class HuffmanHandler : public CompressionHandler
{
public:
    HuffmanHandler();
    ~HuffmanHandler() override;

    bool Compression(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) override;
    bool Decompression(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) override;
    std::uint16_t GetID() const override;
private:
    struct Node
    {
        uint64_t freq;
        int symbol;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        Node(uint64_t f, int s)
            : freq(f), symbol(s), left(nullptr), right(nullptr) {}

        bool isLeaf() const { return !left && !right; }
    };

    struct NodeCmp
    {
        bool operator()(const Node* a, const Node* b) const
        {
            return a->freq > b->freq;
        }
    };

    static void buildCodes(const Node* node,
                           uint32_t code,
                           int depth,
                           std::array<uint32_t, 256>& codeTable,
                           std::array<uint8_t, 256>& lenTable);

    static std::unique_ptr<Node> buildTree(const std::array<uint64_t, 256>& freq);

    class BitWriter
    {
    public:
        explicit BitWriter(std::vector<uint8_t>& buf) : buf_(buf) {}
        void write(uint32_t code, int len);
        void flush();

    private:
        std::vector<uint8_t>& buf_;
        uint8_t cur_ = 0;
        int bitCount_ = 0;
    };

    class BitReader
    {
    public:
        BitReader(const std::vector<uint8_t>& buf, size_t startOffset)
            : buf_(buf), pos_(startOffset) {}

        int read();

    private:
        const std::vector<uint8_t>& buf_;
        size_t pos_;
        uint8_t cur_ = 0;
        int bitCount_ = 0;
    };
};