#include "CommandEntry.h"
#include <sstream>
#include <cstdlib>

#if defined(__GNUC__) || defined(__clang__)
    #include <cxxabi.h>
#endif

CommandEntry::CommandEntry(std::string name, std::string description)
    : name(std::move(name)), description(std::move(description)) {

}

CommandEntry::~CommandEntry(){

}

const std::string& CommandEntry::GetName() const { 
    return name; 
}

const std::string& CommandEntry::GetDescription() const { 
    return description; 
}

const std::vector<std::type_index>& CommandEntry::GetTypes() const { 
    return types; 
}

size_t CommandEntry::GetArity() const { return 
    arity; 
}

bool CommandEntry::IsRegistered() const { 
    return registered; 
}

bool CommandEntry::ExecuteAny(const std::vector<std::any>& args) const {
    if (!registered || !callback) {
        return false;
    }
    if (args.size() != arity) {
        return false;
    }
    return callback(args);
}

bool CommandEntry::IsMatch(const std::vector<std::type_index>& otherTypes) const {
    if (otherTypes.size() != types.size())
        return false;
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i] != otherTypes[i])
            return false;
    }
    return true;
}

std::string CommandEntry::ToString() const {
    std::ostringstream oss;
    std::vector<std::string> paramNames;
    oss << name;
    
    if (registered && !types.empty()) {
        if (paramNames.empty()) {
            for (const auto& type : types) {
                paramNames.push_back(ReadableTypeName(type));
            }
        }
        
        for (const auto& param : paramNames) {
            oss << " " << param;
        }
    }
    
    oss << " | " << description;
    return oss.str();
}

std::string CommandEntry::ReadableTypeName(const std::type_index& idx) {
    const char* raw = idx.name();
    if (!raw) return "unknown";

#if defined(__GNUC__) || defined(__clang__)
        int status = 0;
        char* demangled = abi::__cxa_demangle(raw, nullptr, nullptr, &status);
        if (status == 0 && demangled) {
            std::string result(demangled);
            std::free(demangled);
            return result;
        }
        return raw;

#elif defined(_MSC_VER)
        std::string result(raw);
        for (const char* prefix : {"class ", "struct ", "enum ", "union "}) {
            if (result.find(prefix) == 0) {
                result.erase(0, std::strlen(prefix));
                break;
            }
        }
        return result;

#else
        return raw;
#endif
    }