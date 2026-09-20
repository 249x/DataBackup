#pragma once

#include "CompressionHandler.h"
#include <cstdint>
#include <vector>

class RleHandler : public CompressionHandler
{
public:
    RleHandler();
    ~RleHandler() override;

    bool Compression(const std::vector<uint8_t>& input,
                     std::vector<uint8_t>& output) override;

    bool Decompression(const std::vector<uint8_t>& input,
                       std::vector<uint8_t>& output) override;

    std::uint16_t GetID() const override;
};