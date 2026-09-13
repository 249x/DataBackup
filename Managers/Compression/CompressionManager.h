#pragma once

#include "../../System.h"
#include "../Manager.h"
#include "Handler/CompressionHandler.h"
#include <unordered_map>


class CompressionManager : public Manager
{
public:
    CompressionManager(System& sys);
    ~CompressionManager() override;

    void Initialize() override;

    bool Compression(const std::vector<uint8_t>& input, const std::uint16_t type, std::vector<uint8_t>& output) const;
	bool Decompression(const std::vector<uint8_t>& input, const std::uint16_t type, std::vector<uint8_t>& output) const;
    CompressionHandler* Get(std::uint16_t type) const;

    template <typename T>
    CompressionHandler* Register() {
        static_assert(std::is_base_of_v<CompressionHandler, T>);
        auto ptr = std::make_unique<T>();
        CompressionHandler* handler = ptr.get();
        std::uint16_t id = handler->GetID();

        auto it = handlers.find(id);
        if (it != handlers.end()) {
            return it->second.get();
        }

        handlers.emplace(id, std::move(ptr));
        return handler;
    }

private:

    std::unordered_map<std::uint16_t, std::unique_ptr<CompressionHandler>> handlers;
};