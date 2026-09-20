#include "HuffmanHandler.h"

#include <vector>
#include <algorithm>

// ============ 构造 / 析构 ============
HuffmanHandler::HuffmanHandler() = default;
HuffmanHandler::~HuffmanHandler() = default;

// ============ ID ============
std::uint16_t HuffmanHandler::GetID() const
{
    return 1;
}

// ============ 位写入（MSB first）============
void HuffmanHandler::WriteBit(std::vector<uint8_t>& out,
                              uint8_t& cur, int& bitCount, bool bit)
{
    cur = static_cast<uint8_t>((cur << 1) | (bit ? 1 : 0));
    ++bitCount;
    if (bitCount == 8)
    {
        out.push_back(cur);
        cur = 0;
        bitCount = 0;
    }
}

// ============ 位读取（MSB first）============
bool HuffmanHandler::ReadBit(const std::vector<uint8_t>& in,
                             size_t& bytePos, int& bitPos)
{
    if (bytePos >= in.size()) return false;
    bool bit = (in[bytePos] >> (7 - bitPos)) & 1;
    ++bitPos;
    if (bitPos == 8)
    {
        bitPos = 0;
        ++bytePos;
    }
    return bit;
}

// ============ 构建霍夫曼树 ============
std::unique_ptr<HuffmanHandler::Node>
HuffmanHandler::BuildTree(const std::vector<uint8_t>& input,
                          std::unordered_map<uint8_t, uint64_t>& freq) const
{
    for (uint8_t b : input)
        freq[b]++;

    // 用 vector + 手写堆，避免 priority_queue 的 const_cast 问题
    std::vector<std::unique_ptr<Node>> heap;
    heap.reserve(freq.size());
    for (auto& [byte, count] : freq)
    {
        auto n = std::make_unique<Node>();
        n->byte = byte;
        n->freq = count;
        heap.push_back(std::move(n));
    }

    auto cmp = [](const std::unique_ptr<Node>& a,
                  const std::unique_ptr<Node>& b) {
        return a->freq > b->freq;
    };
    std::make_heap(heap.begin(), heap.end(), cmp);

    // 单字节特判：构造内部根，左孩子为唯一叶子
    if (heap.size() == 1)
    {
        auto root = std::make_unique<Node>();
        root->byte = 0;
        root->freq = heap[0]->freq;
        root->left = std::move(heap[0]);
        root->right = nullptr;
        return root;
    }

    while (heap.size() > 1)
    {
        std::pop_heap(heap.begin(), heap.end(), cmp);
        auto a = std::move(heap.back());
        heap.pop_back();

        std::pop_heap(heap.begin(), heap.end(), cmp);
        auto b = std::move(heap.back());
        heap.pop_back();

        auto parent = std::make_unique<Node>();
        parent->byte = 0;
        parent->freq = a->freq + b->freq;
        parent->left  = std::move(a);
        parent->right = std::move(b);

        heap.push_back(std::move(parent));
        std::push_heap(heap.begin(), heap.end(), cmp);
    }

    return std::move(heap[0]);
}

// ============ 生成编码表 ============
void HuffmanHandler::BuildCodeTable(
    const Node* node,
    const std::vector<bool>& path,
    std::unordered_map<uint8_t, std::vector<bool>>& table) const
{
    if (!node) return;
    if (node->IsLeaf())
    {
        // 单节点树（只有 root->left 是叶子）的特例：给一个 0 位编码
        table[node->byte] = path.empty() ? std::vector<bool>{false} : path;
        return;
    }
    auto leftPath = path;
    leftPath.push_back(false);
    BuildCodeTable(node->left.get(), leftPath, table);

    auto rightPath = path;
    rightPath.push_back(true);
    BuildCodeTable(node->right.get(), rightPath, table);
}

// ============ 序列化霍夫曼树 ============
// 格式：叶子 -> 0x01 + byte；内部节点 -> 0x00
void HuffmanHandler::SerializeTree(const Node* node,
                                   std::vector<uint8_t>& out) const
{
    if (!node) return;
    if (node->IsLeaf())
    {
        out.push_back(0x01);
        out.push_back(node->byte);
        return;
    }
    out.push_back(0x00);
    SerializeTree(node->left.get(), out);
    SerializeTree(node->right.get(), out);
}

std::unique_ptr<HuffmanHandler::Node>
HuffmanHandler::DeserializeTree(const std::vector<uint8_t>& in,
                                size_t& pos) const
{
    if (pos >= in.size()) return nullptr;
    uint8_t tag = in[pos++];
    if (tag == 0x01)
    {
        if (pos >= in.size()) return nullptr;
        auto leaf = std::make_unique<Node>();
        leaf->byte = in[pos++];
        leaf->freq = 0;
        return leaf;
    }
    // tag == 0x00：内部节点
    auto node = std::make_unique<Node>();
    node->left  = DeserializeTree(in, pos);
    node->right = DeserializeTree(in, pos);
    if (!node->left || !node->right) return nullptr;
    return node;
}

// ============ 压缩 ============
bool HuffmanHandler::Compression(const std::vector<uint8_t>& input,
                                 std::vector<uint8_t>& output)
{
    output.clear();

    // 空输入：输出空
    if (input.empty())
        return true;

    // 1. 统计频率 + 建树
    std::unordered_map<uint8_t, uint64_t> freq;
    auto root = BuildTree(input, freq);

    // 2. 生成编码表
    std::unordered_map<uint8_t, std::vector<bool>> table;
    std::vector<bool> path;
    BuildCodeTable(root.get(), path, table);

    // 3. 序列化树
    std::vector<uint8_t> treeBytes;
    SerializeTree(root.get(), treeBytes);

    // 4. 写头部：原始长度(8 字节小端) + 树长度(4 字节小端)
    uint64_t originalSize = input.size();
    for (int i = 0; i < 8; ++i)
        output.push_back(static_cast<uint8_t>((originalSize >> (8 * i)) & 0xFF));

    uint32_t treeSize = static_cast<uint32_t>(treeBytes.size());
    for (int i = 0; i < 4; ++i)
        output.push_back(static_cast<uint8_t>((treeSize >> (8 * i)) & 0xFF));

    // 5. 写树
    output.insert(output.end(), treeBytes.begin(), treeBytes.end());

    // 6. 写位流
    uint8_t cur = 0;
    int bitCount = 0;
    for (uint8_t b : input)
        for (bool bit : table[b])
            WriteBit(output, cur, bitCount, bit);

    // 最后不足 8 位补 0
    if (bitCount > 0)
    {
        cur = static_cast<uint8_t>(cur << (8 - bitCount));
        output.push_back(cur);
    }

    return true;
}

// ============ 解压 ============
bool HuffmanHandler::Decompression(const std::vector<uint8_t>& input,
                                   std::vector<uint8_t>& output)
{
    output.clear();
    if (input.empty())
        return true;

    // 1. 读原始长度
    if (input.size() < 12) return false;
    uint64_t originalSize = 0;
    for (int i = 0; i < 8; ++i)
        originalSize |= static_cast<uint64_t>(input[i]) << (8 * i);

    // 2. 读树长度
    uint32_t treeSize = 0;
    for (int i = 0; i < 4; ++i)
        treeSize |= static_cast<uint32_t>(input[8 + i]) << (8 * i);

    if (input.size() < 12ull + treeSize) return false;

    // 3. 反序列化树
    size_t treePos = 12;
    auto root = DeserializeTree(input, treePos);
    if (!root) return false;

    // 4. 位流起点：用反序列化实际消耗的位置
    size_t bytePos = treePos;
    int bitPos = 0;

    // 5. 按树遍历解码
    const Node* node = root.get();
    while (output.size() < originalSize)
    {
        if (bytePos * 8 + bitPos >= input.size() * 8) break;

        bool bit = ReadBit(input, bytePos, bitPos);
        node = bit ? node->right.get() : node->left.get();
        if (!node) return false;

        if (node->IsLeaf())
        {
            output.push_back(node->byte);
            node = root.get();
        }
    }

    return output.size() == originalSize;
}