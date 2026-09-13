#include "FileIOManager.h"

#include "IO/FileContentIO.h"
#include "IO/FileMetaDataIO.h"
#include "IO/WindowsFileMetaDataIO.h"
#include "../Command/CommandManager.h"

#include <memory>
namespace fs = std::filesystem;

FileIOManager::FileIOManager(System& sys) : Manager(sys)
{
}

FileIOManager::~FileIOManager()
{
}

void FileIOManager::Initialize() {
    contentIO = std::make_unique<FileContentIO>();
    metadataIO = std::make_unique<WindowsFileMetaDataIO>();

    CommandManager* command = Get<CommandManager>();

    command->RegisterCommand("cd", "Change current directory", CD, this);
}

bool FileIOManager::Read(const fs::path& filePath, FileEntry& entry) const {
    if (!contentIO || !metadataIO) {
        return false;
    }
    return contentIO->Read(filePath, entry.Content()) &&
           metadataIO->Read(filePath, entry.MetaData());
}

bool FileIOManager::Read(const fs::path& inputPath,
                         std::vector<FileEntry>& entries) const {
    if (!contentIO || !metadataIO) {
        return false;
    }

    std::error_code error;
    const fs::file_status status = fs::status(inputPath, error);
    if (!fs::is_regular_file(status) && !fs::is_directory(status)) {
        return false;
    }

    if (fs::is_regular_file(status)) {
        FileEntry entry;
        if (!Read(inputPath, entry)) {
            return false;
        }
        entries.emplace_back(std::move(entry));
        return true;
    }

    for (const fs::directory_entry& file : fs::recursive_directory_iterator(
             inputPath, fs::directory_options::skip_permission_denied, error)) {
        if (!file.is_regular_file(error)) {
            continue;
        }
        FileEntry entry;
        if (!Read(file.path(), entry)) {
            return false;
        }
        entry.SetPath(fs::relative(file.path(), inputPath, error));
        entries.emplace_back(std::move(entry));
    }

    return true;
}

bool FileIOManager::Write(const fs::path& filePath, const FileEntry& entry) const {
    if (!contentIO || !metadataIO) {
        return false;
    }

    return contentIO->Write(filePath, entry.Content()) &&
           metadataIO->Write(filePath, entry.MetaData());
}

bool FileIOManager::Write(const fs::path& outputPath,
                          const std::vector<FileEntry>& entries) const {
    if (!contentIO || !metadataIO || entries.empty()) {
        return entries.empty();
    }

    std::error_code error;
    if(!fs::is_directory(outputPath, error)){
        if (entries.size() == 1) {
            return Write(outputPath, entries.front());
        }
        return false;
    }


    fs::create_directories(outputPath, error);

    for (const FileEntry& entry : entries) {
        const fs::path relativePath = entry.Path();
        if (relativePath.empty() || relativePath.is_absolute() ||
            relativePath == fs::path(".")) {
            return false;
        }
        for (const fs::path& component : relativePath) {
            if (component == fs::path("..")) {
                return false;
            }
        }
        const fs::path targetPath = outputPath / relativePath;
        fs::create_directories(targetPath.parent_path(), error);
        if (!Write(targetPath, entry)) {
            return false;
        }
    }
    return true;
}

bool FileIOManager::ReadText(const fs::path& filePath, std::string& text) const{
    if (!contentIO){
        return false;
    }
    return contentIO->ReadText(filePath, text);
}

bool FileIOManager::WriteText(const fs::path& filePath, const std::string& text) const{
    if (!contentIO){
        return false;
    }
    return contentIO->WriteText(filePath, text);
}

fs::path FileIOManager::GetCurrentPath() {
    return std::filesystem::current_path();
}

bool FileIOManager::SetCurrentPath(const fs::path& path) {
    std::error_code ec;
    std::filesystem::current_path(path, ec);
    return !ec;
}

fs::path FileIOManager::Join(const fs::path& sub) {
    return GetCurrentPath() / sub;
}

fs::path FileIOManager::ToRelative(const fs::path& absolute) {
    std::error_code ec;
    auto rel = std::filesystem::relative(absolute, ec);
    return ec ? std::filesystem::path{} : rel;
}

fs::path FileIOManager::ToAbsolute(const fs::path& relative) {
    return fs::absolute(relative);
}

bool FileIOManager::GoToParent() {
    auto parent = fs::current_path().parent_path();
    if (parent.empty()) {
        return false;
    }
    return SetCurrentPath(parent);
}

bool FileIOManager::CD(const std::string& target) {
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