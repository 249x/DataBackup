#include "Debug.h"
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <ctime>

ErrorEntry::ErrorEntry(const std::string& t, const std::string& msg, ErrorLevel lvl)
    : tag(t)
    , message(msg)
    , timestamp(std::chrono::system_clock::now())
    , level(lvl)
{
}

std::vector<ErrorEntry>& Debug::getErrors() {
    return errors;
}

std::mutex& Debug::getMutex() {
    return mutex;
}

void Debug::Log(const std::string& message, const std::string& tag, ErrorLevel level) {
    std::lock_guard<std::mutex> lock(getMutex());
    ErrorEntry entry(tag, message, level);
    getErrors().push_back(entry);
    
    if (level >= ErrorLevel::Warning) {
        std::cerr << formatEntry(entry) << std::endl;
    }
    else{
        std::cout << formatEntry(entry) << std::endl;
    }
}

void Debug::Info(const std::string& message, const std::string& tag) {
    Log(message, tag, ErrorLevel::Info);
}

void Debug::Warning(const std::string& message, const std::string& tag) {
    Log(message, tag, ErrorLevel::Warning);
}

void Debug::Error(const std::string& message, const std::string& tag) {
    Log(message, tag, ErrorLevel::Error);
}

void Debug::Critical(const std::string& message, const std::string& tag) {
    Log(message, tag, ErrorLevel::Critical);
}

std::vector<ErrorEntry> Debug::GetAllErrors() {
    std::lock_guard<std::mutex> lock(getMutex());
    return getErrors();
}

std::vector<ErrorEntry> Debug::GetErrorsByTag(const std::string& tag) {
    std::lock_guard<std::mutex> lock(getMutex());
    std::vector<ErrorEntry> result;
    for (const auto& entry : getErrors()) {
        if (entry.tag == tag) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<ErrorEntry> Debug::GetLastErrors(size_t count) {
    std::lock_guard<std::mutex> lock(getMutex());
    const auto& errors = getErrors();
    size_t start = errors.size() > count ? errors.size() - count : 0;
    return std::vector<ErrorEntry>(errors.begin() + start, errors.end());
}

void Debug::Clear() {
    std::lock_guard<std::mutex> lock(getMutex());
    getErrors().clear();
}

void Debug::ClearByTag(const std::string& tag) {
    std::lock_guard<std::mutex> lock(getMutex());
    auto& errors = getErrors();
    errors.erase(
        std::remove_if(errors.begin(), errors.end(),
            [&tag](const ErrorEntry& entry) { return entry.tag == tag; }),
        errors.end()
    );
}

std::string Debug::ToStringByTag(const std::string& tag) {
    auto entries = GetErrorsByTag(tag);
    if (entries.empty()) {
        return "[No errors for tag: " + tag + "]";
    }
    
    std::ostringstream oss;
    oss << "=== Errors for tag: " << tag << " ===" << std::endl;
    oss << "Total: " << entries.size() << " entries" << std::endl << std::endl;
    
    for (size_t i = 0; i < entries.size(); ++i) {
        oss << formatEntry(entries[i], i + 1, entries.size());
        if (i < entries.size() - 1) {
            oss << std::endl << "---" << std::endl;
        }
    }
    
    return oss.str();
}

std::string Debug::ToStringAll() {
    auto errors = GetAllErrors();
    if (errors.empty()) {
        return "[No errors recorded]";
    }
    
    std::ostringstream oss;
    oss << "=== All Error Logs ===" << std::endl;
    oss << "Total: " << errors.size() << " entries" << std::endl << std::endl;
    
    for (size_t i = 0; i < errors.size(); ++i) {
        oss << formatEntry(errors[i], i + 1, errors.size());
        if (i < errors.size() - 1) {
            oss << std::endl << "---" << std::endl;
        }
    }
    
    return oss.str();
}

std::string Debug::ToJson(const std::string& tag) {
    auto entries = tag.empty() ? GetAllErrors() : GetErrorsByTag(tag);
    
    std::ostringstream oss;
    oss << "{";
    oss << "\"total\":" << entries.size() << ",";
    oss << "\"errors\":[";
    
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"tag\":\"" << escapeJson(entries[i].tag) << "\",";
        oss << "\"message\":\"" << escapeJson(entries[i].message) << "\",";
        oss << "\"level\":\"" << LevelToString(entries[i].level) << "\",";
        oss << "\"time\":\"" << formatTime(entries[i].timestamp) << "\"";
        oss << "}";
    }
    
    oss << "]}";
    return oss.str();
}

size_t Debug::GetErrorCount() {
    std::lock_guard<std::mutex> lock(getMutex());
    return getErrors().size();
}

size_t Debug::GetErrorCountByTag(const std::string& tag) {
    std::lock_guard<std::mutex> lock(getMutex());
    size_t count = 0;
    for (const auto& entry : getErrors()) {
        if (entry.tag == tag) {
            ++count;
        }
    }
    return count;
}

bool Debug::HasErrors() {
    std::lock_guard<std::mutex> lock(getMutex());
    return !getErrors().empty();
}

bool Debug::HasErrorsAbove(ErrorLevel level) {
    std::lock_guard<std::mutex> lock(getMutex());
    for (const auto& entry : getErrors()) {
        if (entry.level >= level) {
            return true;
        }
    }
    return false;
}

std::string Debug::LevelToString(ErrorLevel level) {
    switch (level) {
        case ErrorLevel::Info:     return "INFO";
        case ErrorLevel::Warning:  return "WARNING";
        case ErrorLevel::Error:    return "ERROR";
        case ErrorLevel::Critical: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string Debug::formatEntry(const ErrorEntry& entry, 
                                      size_t index, 
                                      size_t total) {
    std::ostringstream oss;
    
    if (index > 0 && total > 0) {
        oss << "[" << index << "/" << total << "] ";
    }
    
    oss << "[" << LevelToString(entry.level) << "] ";
    oss << "[" << formatTime(entry.timestamp) << "] ";
    oss << "[" << entry.tag << "] ";
    oss << entry.message;
    
    return oss.str();
}

std::string Debug::formatTime(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch()
    ).count() % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setw(3) << std::setfill('0') << ms;
    return oss.str();
}

std::string Debug::escapeJson(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (c < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
                break;
        }
    }
    return oss.str();
}