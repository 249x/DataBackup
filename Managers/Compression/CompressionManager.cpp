#include "CompressionManager.h"
#include "Handler/HuffmanHandler.h"

CompressionManager::CompressionManager(System& sys) : Manager(sys){
}

CompressionManager::~CompressionManager(){
    
}

void CompressionManager::Initialize() {
    Register<HuffmanHandler>();
}


bool CompressionManager::Compression(const std::vector<uint8_t>& input, const std::uint16_t type, std::vector<uint8_t>& output) const{
    CompressionHandler* handler = Get(type);
    if(handler == nullptr){
        return false;
    }
    output.clear();
    handler->Compression(input, output);
    return true;
}

bool CompressionManager::Decompression(const std::vector<uint8_t>& input, const std::uint16_t type, std::vector<uint8_t>& output)const{
    CompressionHandler* handler = Get(type);
    if (handler == nullptr) {
        return false;
    }
    output.clear();
    return handler->Decompression(input, output);
}

CompressionHandler* CompressionManager::Get(std::uint16_t type) const{
    auto it = handlers.find(type);
    if (it == handlers.end()) return nullptr;
    return it->second.get();
}