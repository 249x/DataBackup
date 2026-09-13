#pragma once

#include "../Manager.h"
#include "../../FileStruct/FileEntry.h"

#include <filesystem>
#include <memory>
#include <vector>

class FileContentIO;
class FileMetaDataIO;

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

    bool ReadText(const std::filesystem::path& filePath, std::string& text) const;
    bool WriteText(const std::filesystem::path& filePath, const std::string& text) const;

    std::filesystem::path GetCurrentPath();
    bool SetCurrentPath(const std::filesystem::path& path);
    bool CD(const std::string& target);
    std::filesystem::path Join(const std::filesystem::path& sub);
    bool GoToParent();
    
    std::filesystem::path ToAbsolute(const std::filesystem::path& relative);
    std::filesystem::path ToRelative(const std::filesystem::path& absolute);

private:
    std::unique_ptr<FileContentIO> contentIO;
    std::unique_ptr<FileMetaDataIO> metadataIO;
};          