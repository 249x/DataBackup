#include "FileIOManager.h"

#include "IO/Content/FileContentIO.h"
#include "IO/Meta/FileMetaDataIO.h"
#include "IO/Meta/WindowsFileMetaDataIO.h"
#include "IO/PathHandler.h"
#include "../Command/CommandManager.h"
#include "../../General/Debug.h"

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
    pathHandler = std::make_unique<PathHandler>();

    CommandManager* command = Get<CommandManager>();

    command->RegisterCommand("cd", "Change current directory", CD, this);
}

bool FileIOManager::Read(const fs::path& filePath, FileEntry& entry) const {
    if (!contentIO || !metadataIO) {
        Debug::Error("No files reader exists", "FileIO");
        return false;
    }

    if (!metadataIO->Read(filePath, entry.MetaData())) {
        return false;
    }

    return contentIO->Read(filePath, entry.Content(), entry.MetaData().Type());
}

bool FileIOManager::Read(const fs::path& inputPath,
                         std::vector<FileEntry>& entries) const {
    if (!contentIO || !metadataIO) {
        Debug::Error("No file reader exist", "FileIO");
        return false;
    }

    std::error_code error;
    const fs::file_status status = fs::status(inputPath, error);

    // 读取单个文件
    if (!fs::is_directory(status)) {
        FileEntry entry;
        if (!Read(inputPath, entry)) {
            return false;
        }
        entries.emplace_back(std::move(entry));
        return true;
    }

    const fs::recursive_directory_iterator end;
    for (auto it = fs::recursive_directory_iterator(
             inputPath, fs::directory_options::skip_permission_denied, error);
         it != end; ++it) {
        const fs::directory_entry& file = *it;

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
        Debug::Error("No files writer exist", "FileIO");
        return false;
    }

    return contentIO->Write(filePath, entry.Content(), entry.MetaData().Type()) &&
           metadataIO->Write(filePath, entry.MetaData());
}

bool FileIOManager::IsSafeRelativePath(const fs::path& path) const {
    if (path.empty() || path.is_absolute() || path == fs::path(".")) {
        return false;
    }
    for (const fs::path& component : path) {
        if (component == fs::path("..")) {
            return false;
        }
    }
    return true;
}

bool FileIOManager::Write(const fs::path& outputPath,
                          const std::vector<FileEntry>& entries) const {
    if(entries.empty()){
        Debug::Warning("No file for writing", "FileIO");
        return true;
    }

    if (!contentIO || !metadataIO) {
        Debug::Error("No file writer exist", "FileIO");
        return false;
    }
    
    // 处理读取单个文件的情况
    if (entries.size() == 1 && entries.front().Path().empty()) {
        return Write(outputPath, entries.front());
    }
    std::error_code error;
    fs::create_directories(outputPath, error);

    for (const FileEntry& entry : entries) {
        const fs::path relativePath = entry.Path();
        if (!IsSafeRelativePath(relativePath)) {
            Debug::Warning("Unsafe relative path: " + relativePath.string(), "FileIO");
            continue;
        }

        const fs::path targetPath = outputPath / relativePath;
        fs::create_directories(targetPath.parent_path(), error);
        if (!Write(targetPath, entry)) {
            Debug::Warning("Failed to write: " + relativePath.string(), "FileIO");
        }
    }

    return true;
}

bool FileIOManager::ReadContent(const std::filesystem::path& filePath, std::vector<uint8_t>& content) const{
    if (!contentIO) {
        Debug::Error("No files content reader exists", "FileIO");
        return false;
    }
    return contentIO->Read(filePath, content);
}

bool FileIOManager::WriteContent(const std::filesystem::path& filePath, const std::vector<uint8_t>& content) const{
    if (!contentIO || !metadataIO) {
        Debug::Error("No files content writer exist", "FileIO");
        return false;
    }

    return contentIO->Write(filePath, content);
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

fs::path FileIOManager::CurrentPath(){
    return pathHandler->GetCurrentPath();
}

bool FileIOManager::CD(const std::string& target){
    return pathHandler->CD(target);
}