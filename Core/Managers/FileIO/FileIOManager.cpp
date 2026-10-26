#include "FileIOManager.h"

#include "IO/Content/FileContentIO.h"
#include "IO/Content/HardLinkIO.h"
#include "IO/Meta/FileMetaDataIO.h"
#include "IO/Meta/WindowsFileMetaDataIO.h"
#include "IO/PathHandler.h"
#include "../Command/CommandManager.h"
#include "../../General/Debug.h"
#include "../../General/Utf8.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32

void LogDeleteFailure(const fs::path& path, unsigned long code) {
    Debug::Warning("Failed to delete: " + Utf8::FromPath(path) + " (win32 error " +
                       std::to_string(code) + ")",
                   "FileIO");
}

// 只读属性会让删除直接失败（备份里保留了只读标记），先摘掉它
void MakeWritable(const fs::path& path) {
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0) {
        ::SetFileAttributesW(path.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
    }
}

// 删单个条目：目录用 RemoveDirectoryW（只能删空目录），其余用 DeleteFileW
bool RemoveSingle(const fs::path& path, bool directory) {
    MakeWritable(path);
    if (directory) {
        return ::RemoveDirectoryW(path.c_str()) != FALSE;
    }
    return ::DeleteFileW(path.c_str()) != FALSE;
}

// 递归删除。重解析点（符号链接 / junction）只删链接本身：本工具链的 std::filesystem 把
// junction 当普通目录，跟进目标会把链接指向的内容一起删掉，所以这里靠属性位判断。
bool DeleteTree(const fs::path& path) {
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        LogDeleteFailure(path, ::GetLastError());
        return false;
    }

    const bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (!directory || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        if (RemoveSingle(path, directory)) {
            return true;
        }
        LogDeleteFailure(path, ::GetLastError());
        return false;
    }

    // 先把孩子收齐再删：边遍历边删会让迭代器悬空
    std::vector<fs::path> children;
    std::error_code error;
    for (const fs::directory_entry& child : fs::directory_iterator(path, error)) {
        children.push_back(child.path());
    }
    if (error) {
        LogDeleteFailure(path, static_cast<unsigned long>(error.value()));
        return false;
    }

    for (const fs::path& child : children) {
        if (!DeleteTree(child)) {
            return false;
        }
    }
    if (RemoveSingle(path, true)) {
        return true;
    }
    LogDeleteFailure(path, ::GetLastError());
    return false;
}

#else

bool DeleteTree(const fs::path& path) {
    std::error_code error;
    fs::remove_all(path, error); // POSIX 下 remove_all 本来就不跟进符号链接
    if (error) {
        Debug::Warning("Failed to delete: " + Utf8::FromPath(path), "FileIO");
        return false;
    }
    return true;
}

#endif

} // namespace

FileIOManager::FileIOManager(System& sys) : Manager(sys)
{
}

FileIOManager::~FileIOManager()
{
}

void FileIOManager::Initialize() {
    contentIO = std::make_unique<FileContentIO>();
    metadataIO = std::make_unique<WindowsFileMetaDataIO>();
    hardLinkIO = std::make_unique<HardLinkIO>();
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
    if (!contentIO || !metadataIO || !hardLinkIO) {
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
        entry.SetPath(inputPath.filename());
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

        if (entry.MetaData().Type() != FileType::Directory) {
            it.disable_recursion_pending();
        }

        entry.SetPath(fs::relative(file.path(), inputPath, error));
        entries.emplace_back(std::move(entry));
    }

    hardLinkIO->Deduplicate(entries);
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

    if (!contentIO || !metadataIO || !hardLinkIO) {
        Debug::Error("No file writer exist", "FileIO");
        return false;
    }
    // 处理读取单个文件的情况
    if (entries.size() == 1 && entries.front().Path().empty()) {
        return Write(outputPath, entries.front());
    }
    std::error_code error;
    fs::create_directories(outputPath, error);
    // 分三段写：内容与目录结构 -> 硬链接 -> 元数据。
    std::vector<std::optional<fs::path>> targets;
    targets.reserve(entries.size());

    for (const FileEntry& entry : entries) {
        const fs::path relativePath = entry.Path();
        if (!IsSafeRelativePath(relativePath)) {
            Debug::Warning("Unsafe relative path: " + relativePath.string(), "FileIO");
            targets.emplace_back(std::nullopt);
            continue;
        }

        const fs::path targetPath = outputPath / relativePath;
        fs::create_directories(targetPath.parent_path(), error);
        if (!contentIO->Write(targetPath, entry.Content(), entry.MetaData().Type())) {
            Debug::Warning("Failed to write: " + relativePath.string(), "FileIO");
            targets.emplace_back(std::nullopt);
            continue;
        }
        targets.emplace_back(targetPath);
    }

    hardLinkIO->Relink(entries, targets, *contentIO);

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!targets[i].has_value()) {
            continue;
        }
        metadataIO->Write(*targets[i], entries[i].MetaData());
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

bool FileIOManager::Exists(const std::filesystem::path& path) const{
    return pathHandler->CheckPath(path);
}

bool FileIOManager::Delete(const std::filesystem::path& path) const{
    if (path.empty()) {
        Debug::Warning("Nothing to delete: empty path", "FileIO");
        return false;
    }
    return DeleteTree(path);
}

fs::path FileIOManager::CurrentPath(){
    return pathHandler->GetCurrentPath();
}

bool FileIOManager::CD(const std::string& target){
    return pathHandler->CD(target);
}

fs::path FileIOManager::BesideExecutable(const std::filesystem::path& relative) const{
    return pathHandler->BesideExecutable(relative);
}