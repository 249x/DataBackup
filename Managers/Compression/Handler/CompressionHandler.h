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

};

