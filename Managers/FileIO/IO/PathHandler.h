#pragma once


#include <filesystem>
#include <vector>

class FileContentIO;
class FileMetaDataIO;

class PathHandler
{
public:
    PathHandler();
    ~PathHandler();

    std::filesystem::path GetCurrentPath();
    bool SetCurrentPath(const std::filesystem::path& path);
    std::filesystem::path ToAbsolute(const std::filesystem::path& relative);
    std::filesystem::path ToRelative(const std::filesystem::path& absolute);
    bool CD(const std::string& target);
    
    bool CheckPath(const std::filesystem::path& path);
    bool CheckRegular(const std::filesystem::path& path);
private:
};          