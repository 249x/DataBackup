#include "PathHandler.h"

namespace fs = std::filesystem;

PathHandler::PathHandler() = default;
PathHandler::~PathHandler() = default;

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