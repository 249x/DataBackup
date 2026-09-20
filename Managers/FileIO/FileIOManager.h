#pragma once

#include "../Manager.h"
#include "../../FileStruct/FileEntry.h"

#include <filesystem>
#include <memory>
#include <vector>

class FileContentIO;
class FileMetaDataIO;
class PathHandler;

class FileIOManager : public Manager
{
public:
    FileIOManager(System& sys);
    ~FileIOManager() override;

    void Initialize() override;

    bool Read(const std::filesystem::path& filePath, FileEntry& entry) const;
    bool Write(const std::filesystem::path& filePath, const FileEntry& entry) const;
    bool Read(const std::filesystem::path& inputPath, std::vector<FileEntry>& entries) const;
    bool Write(const std::filesystem::path& outputPath, const std::vector<FileEntry>& entries) const;

    bool ReadContent(const std::filesystem::path& filePath, std::vector<uint8_t>& content) const;
    bool WriteContent(const std::filesystem::path& filePath, const std::vector<uint8_t>& content) const;

    bool ReadText(const std::filesystem::path& filePath, std::string& text) const;
    bool WriteText(const std::filesystem::path& filePath, const std::string& text) const;

    std::filesystem::path CurrentPath();
    bool CD(const std::string& target);
private:
    // 拒绝绝对路径、"." 与任何 ".." 分量，避免归档条目写到目标目录之外
    bool IsSafeRelativePath(const std::filesystem::path& path) const;

    std::unique_ptr<FileContentIO> contentIO;
    std::unique_ptr<FileMetaDataIO> metadataIO;
    std::unique_ptr<PathHandler> pathHandler;
};          