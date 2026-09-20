#include "CompressionManager.h"
#include "Handler/HuffmanHandler.h"
#include "Handler/RleHandler.h"
#include "../../General/Debug.h"

CompressionManager::CompressionManager(System& sys) : Manager(sys){
}

CompressionManager::~CompressionManager(){
    
}

void CompressionManager::Initialize() {
    Register<HuffmanHandler>();
    Register<RleHandler>();
}


bool CompressionManager::Compression(const std::vector<uint8_t>& input, const std::uint16_t type, std::vector<uint8_t>& output) const{
    CompressionHandler* handler = Get(type);
    if(handler == nullptr){
        Debug::Error("No compress handler find", "Compression");
        return false;
    }
    output.clear();
    if(!handler->Compression(input, output)){
        Debug::Error("Compress failed", "Compression");
        return false;
    }
    return true;
}

bool CompressionManager::Decompression(const std::vector<uint8_t>& input, const std::uint16_t type, std::vector<uint8_t>& output)const{
    CompressionHandler* handler = Get(type);
    if (handler == nullptr) {
        Debug::Error("No compress handler find", "Compression");
        return false;
    }
    output.clear();
    if(!handler->Decompression(input, output)){
        Debug::Error("Decompress failed", "Compression");
        return false;
    }
    return true;
}

CompressionHandler* CompressionManager::Get(std::uint16_t type) const{
    auto it = handlers.find(type);
    if (it == handlers.end()) return nullptr;
    return it->second.get();
}