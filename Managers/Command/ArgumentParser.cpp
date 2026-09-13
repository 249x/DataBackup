#include "ArgumentParser.h"
#include <cstdint>
#include <string>
#include <cstring>
#include <filesystem>

ArgumentParser::ArgumentParser(){
    registerCommonTypes();
}

ArgumentParser::~ArgumentParser(){

}

bool ArgumentParser::IsRegistered(const std::type_index& type) const{
    return converters.find(type) != converters.end();
}

bool ArgumentParser::Parse(const std::type_index& type, const char* input, std::any& out) const {
    auto it = converters.find(type);
    if (it == converters.end()) {
        return false;
    }
    try {
        out = it->second(input);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ArgumentParser::ParseBatch(const std::vector<const char*>& inputs, 
                    const std::vector<std::type_index>& types,
                    std::vector<std::any>& outValues) const {
    if (inputs.size() != types.size()) {
        return false;
    }
        
    outValues.clear();
    outValues.reserve(inputs.size());
        
    std::any result;
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (!Parse(types[i], inputs[i], result)) {
            outValues.clear();
            return false;
        }
        outValues.push_back(std::move(result));
    }
    return true;
}

void ArgumentParser::registerCommonTypes() {
    Register<int>([](const char* input) -> int {
        return std::stoi(input);
    });

    Register<unsigned int>([](const char* input) -> unsigned int {
        return static_cast<unsigned int>(std::stoul(input));
    });

    Register<long>([](const char* input) -> long {
        return std::stol(input);
    });


    Register<long long>([](const char* input) -> long long {
        return std::stoll(input);
    });

    Register<uint8_t>([](const char* input) -> uint8_t {
        return static_cast<uint8_t>(std::stoul(input));
    });

    Register<uint16_t>([](const char* input) -> uint16_t {
        return static_cast<uint16_t>(std::stoul(input));
    });

    Register<uint32_t>([](const char* input) -> uint32_t {
        return static_cast<uint32_t>(std::stoul(input));
    });

    Register<uint64_t>([](const char* input) -> uint64_t {
        return std::stoull(input);
    });

    Register<float>([](const char* input) -> float {
        return std::stof(input);
    });

    Register<double>([](const char* input) -> double {
        return std::stod(input);
    });

    Register<bool>([](const char* input) -> bool {
        std::string str(input);
        for (char& c : str) {
            c = std::tolower(c);
        }
        if (str == "true" || str == "1" || str == "yes" || str == "on" || str == "y") {
            return true;
        } else if (str == "false" || str == "0" || str == "no" || str == "off" || str == "n") {
            return false;
        }
        return false;
    });

    Register<char>([](const char* input) -> char {
        return input[0];
    });

    Register<std::string>([](const char* input) -> std::string {
        return std::string(input);
    });

    Register<std::filesystem::path>([](const char* input) -> std::filesystem::path {
    return std::filesystem::path(input);
    });

    Register<const char*>([](const char* input) -> const char* {
        return input;
    });

    Register<char*>([](const char* input) -> char* {
        return const_cast<char*>(input);
    });

}

