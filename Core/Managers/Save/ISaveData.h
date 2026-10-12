#pragma once

#include <vector>
#include <cstdint>

class ISaveData
{
public:
    ISaveData() = default;
    ~ISaveData() = default;

    virtual bool Save(std::vector<std::uint8_t>& data) = 0;
    virtual bool Load(const std::vector<std::uint8_t>& data) = 0;
    virtual uint32_t SaveID() const = 0;
};

