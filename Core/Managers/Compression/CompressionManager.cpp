#include "CompressionManager.h"
#include "Handler/HuffmanHandler.h"
#include "Handler/RleHandler.h"
#include "../../General/Debug.h"

#include <algorithm>

CompressionManager::CompressionManager(System& sys) : Manager(sys){
}

CompressionManager::~CompressionManager(){
    
}

void CompressionManager::Initialize() {
    Register<HuffmanHandler>();
    Register<RleHandler>();
}

bool CompressionManager::Compression(const std::vector<uint8_t>& input, std::uint16_t type, std::vector<uint8_t>& output) const{
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

bool CompressionManager::Decompression(const std::vector<uint8_t>& input, std::uint16_t type, std::vector<uint8_t>& output)const{
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

std::vector<AlgorithmInfo> CompressionManager::Algorithms() const {
    std::vector<AlgorithmInfo> list;
    list.reserve(handlers.size());
    for (const auto& pair : handlers) {
        if (pair.second) {
            list.push_back(AlgorithmInfo{pair.first, pair.second->Name(),
                                         pair.second->Description()});
        }
    }
    // unordered_map 的遍历顺序不定，按编号排一下，界面下拉框的顺序才稳定
    std::sort(list.begin(), list.end(),
              [](const AlgorithmInfo& left, const AlgorithmInfo& right) {
                  return left.id < right.id;
              });
    return list;
}

std::uint16_t CompressionManager::IdOf(const std::string& name) const noexcept {
    if (name.empty()) {
        return 0;
    }
    for (const auto& pair : handlers) {
        if (pair.second && name == pair.second->Name()) {
            return pair.first;
        }
    }
    return 0;
}