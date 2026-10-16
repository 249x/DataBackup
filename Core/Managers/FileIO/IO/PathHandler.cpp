#include "PathHandler.h"

#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

#if defined(_WIN32)
// GetModuleFileNameW 会截断，所以给不够就翻倍重试
fs::path QueryExecutablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length =
            ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {}; // 理论上不会发生
        }
        if (length < buffer.size()) {
            return fs::path(std::wstring(buffer.data(), length));
        }
        if (buffer.size() >= 32768) { // Windows 的路径长度上限
            return {};
        }
        buffer.resize(buffer.size() * 2);
    }
}
#endif

} // namespace

PathHandler::PathHandler() = default;
PathHandler::~PathHandler() = default;

fs::path PathHandler::ExecutablePath() const {
#if defined(_WIN32)
    // 同一个进程里它不会变，量一次就够
    static const fs::path cached = QueryExecutablePath();
    return cached;
#else
    return {}; // 其它平台暂不查；BesideExecutable 会退化成相对当前目录
#endif
}

fs::path PathHandler::ExecutableDirectory() const {
    const fs::path exe = ExecutablePath();
    return exe.empty() ? fs::path{} : exe.parent_path();
}

fs::path PathHandler::BesideExecutable(const fs::path& relative) const {
    const fs::path directory = ExecutableDirectory();
    return directory.empty() ? relative : directory / relative;
}

fs::path PathHandler::GetCurrentPath() {
    return std::filesystem::current_path();
}

bool PathHandler::SetCurrentPath(const fs::path& path) {
    std::error_code ec;
    std::filesystem::current_path(path, ec);
    return !ec;
}

fs::path PathHandler::ToRelative(const fs::path& absolute) {
    std::error_code ec;
    auto rel = std::filesystem::relative(absolute, ec);
    return ec ? std::filesystem::path{} : rel;
}

fs::path PathHandler::ToAbsolute(const fs::path& relative) {
    return fs::absolute(relative);
}

bool PathHandler::CheckPath(const fs::path& path) {
    std::error_code ec;
    bool exists = std::filesystem::exists(path, ec);
    if (ec || !exists) {
        return false;
    }

    auto status = std::filesystem::status(path, ec);
    if (ec) return false;
    return true;
}

bool PathHandler::CheckRegular(const fs::path& path){
    std::error_code ec;
    bool exists = std::filesystem::exists(path, ec);
    if (ec || !exists) {
        return false;
    }
    auto status = std::filesystem::status(path, ec);
    return std::filesystem::is_regular_file(status);
}

bool PathHandler::CD(const std::string& target) {
    fs::path newPath;

    if (target.empty() || target == ".") {
        newPath = GetCurrentPath();
    } else if (target == "..") {
        fs::path parent = GetCurrentPath().parent_path();
        if (parent.empty()) {
            newPath = GetCurrentPath();
        } else {
            newPath = parent;
        }
    } else if (target == "~" || target.rfind("~/", 0) == 0) {
        const char* home = std::getenv("HOME");
        if (!home) home = std::getenv("USERPROFILE");
        if (!home) {
            return false;
        }
        fs::path homePath(home);
        if (target == "~") {
            newPath = homePath;
        } else {
            newPath = homePath / target.substr(2);
        }
    } else {
        newPath = ToAbsolute(fs::path(target));
    }

    if (!SetCurrentPath(newPath)) {
        return false;
    }
    return true;
}