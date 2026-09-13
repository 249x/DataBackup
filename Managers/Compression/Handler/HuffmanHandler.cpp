#include "HuffmanHandler.h"
#include <functional>

HuffmanHandler::HuffmanHandler() = default;
HuffmanHandler::~HuffmanHandler() = default;

void HuffmanHandler::BitWriter::write(uint32_t code, int len)
{
    for (int i = len - 1; i >= 0; --i){
        uint8_t bit = static_cast<uint8_t>((code >> i) & 1u);
        cur_ = static_cast<uint8_t>((cur_ << 1) | bit);
        if (++bitCount_ == 8){
            buf_.push_back(cur_);
            cur_ = 0;
            bitCount_ = 0;
        }
    }
}

void HuffmanHandler::BitWriter::flush()
{
    if (bitCount_ > 0){
        cur_ = static_cast<uint8_t>(cur_ << (8 - bitCount_));
        buf_.push_back(cur_);
        cur_ = 0;
        bitCount_ = 0;
    }
}

int HuffmanHandler::BitReader::read()
{
    if (bitCount_ == 0){
        if (pos_ >= buf_.size()) return -1;
        cur_ = buf_[pos_++];
        bitCount_ = 8;
    }
    int bit = (cur_ >> (bitCount_ - 1)) & 1;
    --bitCount_;
    return bit;
}

void HuffmanHandler::buildCodes(const Node* node,
                                uint32_t code,
                                int depth,
                                std::array<uint32_t, 256>& codeTable,
                                std::array<uint8_t, 256>& lenTable)
{
    if (!node) return;
    if (node->isLeaf())
    {
        codeTable[node->symbol] = code;
        lenTable[node->symbol] =
            (depth == 0) ? 1 : static_cast<uint8_t>(depth);
        return;
    }
    buildCodes(node->left.get(),  (code << 1) | 0u, depth + 1, codeTable, lenTable);
    buildCodes(node->right.get(), (code << 1) | 1u, depth + 1, codeTable, lenTable);
}

std::unique_ptr<HuffmanHandler::Node>
HuffmanHandler::buildTree(const std::array<uint64_t, 256>& freq)
{
    using NodePtr = std::unique_ptr<Node>;

    auto cmp = [](const Node* a, const Node* b) { return a->freq > b->freq; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> pq(cmp);

    std::vector<NodePtr> pool;

    for (int i = 0; i < 256; ++i){
        if (freq[i] > 0){
            pool.push_back(std::make_unique<Node>(freq[i], i));
            pq.push(pool.back().get());
        }
    }

    if (pq.empty()) return nullptr;
    if (pq.size() == 1){
        Node* only = pq.top();
        for (auto& p : pool){
            if (p.get() == only)
                return std::move(p);
        }
        return nullptr;
    }

    while (pq.size() > 1)
    {
        Node* a = pq.top(); pq.pop();
        Node* b = pq.top(); pq.pop();

        auto parent = std::make_unique<Node>(a->freq + b->freq, -1);
        for (auto& p : pool){
            if (p.get() == a){
                parent->left = std::move(p);
                break;
            }
        }
        for (auto& p : pool){
            if (p.get() == b){
                parent->right = std::move(p);
                break;
            }
        }

        Node* raw = parent.get();
        pool.push_back(std::move(parent));
        pq.push(raw);
    }

    Node* rootRaw = pq.top();
    for (auto& p : pool)
    {
        if (p.get() == rootRaw)
            return std::move(p);
    }
    return nullptr;
}

// ============================================================
// Compression
// 头部格式：
//   [8 字节原始长度 (小端)]
//   [256 * 4 字节频率 (小端, uint32_t)]
//   [压缩位流]
// ============================================================
bool HuffmanHandler::Compression(const std::vector<uint8_t>& input,
                                 std::vector<uint8_t>& output)
{
    output.clear();
    // 1. 统计频率
    std::array<uint64_t, 256> freq{};
    for (uint8_t b : input) freq[b]++;
    
    // 2. 写头部
    uint64_t originalSize = input.size();
    for (int i = 0; i < 8; ++i)
        output.push_back(static_cast<uint8_t>((originalSize >> (i * 8)) & 0xFF));

    for (int i = 0; i < 256; ++i){
        uint32_t f = static_cast<uint32_t>(freq[i]);
        for (int j = 0; j < 4; ++j)
            output.push_back(static_cast<uint8_t>((f >> (j * 8)) & 0xFF));
    }
    if (input.empty()) return true;

    // 3. 建树
    std::unique_ptr<Node> root = buildTree(freq);
    if (!root) return false;

    // 4. 生成码表
    std::array<uint32_t, 256> codeTable{};
    std::array<uint8_t, 256> lenTable{};
    if (root->isLeaf()){
        codeTable[root->symbol] = 0;
        lenTable[root->symbol] = 1;
    }
    else{
        buildCodes(root.get(), 0, 0, codeTable, lenTable);
    }

    // 5. 编码位流
    BitWriter bw(output);
    for (uint8_t b : input)
        bw.write(codeTable[b], lenTable[b]);
    bw.flush();

    return true;
}

bool HuffmanHandler::Decompression(const std::vector<uint8_t>& input,
                                   std::vector<uint8_t>& output)
{
    output.clear();

    constexpr size_t HEADER_SIZE = 8 + 256 * 4;   // 1032 字节
    if (input.size() < HEADER_SIZE) return false;

    // 1. 读原始长度
    uint64_t originalSize = 0;
    for (int i = 0; i < 8; ++i)
        originalSize |= static_cast<uint64_t>(input[i]) << (i * 8);

    if (originalSize == 0) return true;

    // 2. 读频率表
    std::array<uint64_t, 256> freq{};
    for (int i = 0; i < 256; ++i)
    {
        uint32_t f = 0;
        for (int j = 0; j < 4; ++j)
            f |= static_cast<uint32_t>(input[8 + i * 4 + j]) << (j * 8);
        freq[i] = f;
    }

    // 3. 重建 Huffman 树
    std::unique_ptr<Node> root = buildTree(freq);
    if (!root) return false;

    output.reserve(static_cast<size_t>(originalSize));

    // 4. 只有一种符号
    if (root->isLeaf())
    {
        for (uint64_t i = 0; i < originalSize; ++i)
            output.push_back(static_cast<uint8_t>(root->symbol));
        return true;
    }

    // 5. 逐位解码
    BitReader br(input, HEADER_SIZE);
    const Node* cur = root.get();

    while (output.size() < originalSize){
        int bit = br.read();
        if (bit < 0) return false;

        cur = (bit == 0) ? cur->left.get() : cur->right.get();
        if (!cur) return false;

        if (cur->isLeaf()){
            output.push_back(static_cast<uint8_t>(cur->symbol));
            cur = root.get();
        }
    }

    return true;
}

std::uint16_t HuffmanHandler::GetID() const {
    return 1;
};