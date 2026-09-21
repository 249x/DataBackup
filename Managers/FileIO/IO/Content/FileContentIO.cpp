#include "FileContentIO.h"
#include "ReparsePointIO.h"

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "../../../../General/Debug.h"
#include "../../../../General/Utf8.h"

namespace fs = std::filesystem;

FileContentIO::FileContentIO() = default;
FileContentIO::~FileContentIO() = default;

bool FileContentIO::Read(const fs::path& path, std::vector<std::uint8_t>& content,
                         FileType type) const {
    if (type == FileType::None || IsFileContentType(type)) {
        return ReadRegularFile(path, content);
    }
    if (IsLinkType(type)) {
        return ReparsePointIO::ReadTarget(path, content);
    }
    // 目录、设备、管道、套接字、Other 都没有可搬运的载荷，仅按类型标记
    content.clear();
    return true;
}

bool FileContentIO::Write(const fs::path& path, const std::vector<std::uint8_t>& content,
                          FileType type) const {
    if (type == FileType::None || IsFileContentType(type)) {
        return WriteRegularFile(path, content);
    }

    if (IsLinkType(type)) {
        const std::string target(content.begin(), content.end());
        if (target.empty()) {
            Debug::Error("Link target is empty: " + path.string(), "ContentIO");
            return false;
        }
        return ReparsePointIO::Create(path, Utf8::ToPath(target));
    }

    if (type == FileType::Directory) {
        std::error_code error;
        if (fs::is_directory(path, error)) {
            return true;
        }
        fs::create_directories(path, error);
        if (error) {
            Debug::Error("Failed to create directory: " + path.string() + " (" +
                             error.message() + ")",
                         "ContentIO");
            return false;
        }
        return true;
    }
    if (type == FileType::Socket) {
        return ReparsePointIO::CreateSocket(path);
    }

    // 块 / 字符设备、管道、Other 没有可重建的文件系统形态，落为占位空文件并留痕
    Debug::Warning(std::string("Restoring '") + FileTypeName(type) +
                       "' as an empty placeholder: " + path.string(),
                   "ContentIO");
    return WriteRegularFile(path, content);
}

bool FileContentIO::Read(const fs::path& path, std::vector<std::uint8_t>& content) const {
    return Read(path, content, FileType::Regular);
}

bool FileContentIO::Write(const fs::path& path,
                          const std::vector<std::uint8_t>& content) const {
    return Write(path, content, FileType::Regular);
}

bool FileContentIO::ReadText(const std::filesystem::path& filePath,
                             std::string& text) const {
    if (!std::filesystem::exists(filePath)) {
        Debug::Error("File does not exist", "ContentIO");
        return false;
    }

    if (!std::filesystem::is_regular_file(filePath)) {
        Debug::Error("Path is not a regular file", "ContentIO");
        return false;
    }

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        Debug::Error("Failed to open text file", "ContentIO");
        return false;
    }

    std::ostringstream oss;
    oss << file.rdbuf();

    if (file.fail() && !file.eof()) {
        Debug::Error("Failed to read text file", "ContentIO");
        return false;
    }

    text = oss.str();
    return true;
}

bool FileContentIO::WriteText(const std::filesystem::path& filePath,
                              const std::string& text) const {
    std::filesystem::path parentPath = filePath.parent_path();
    if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(parentPath, ec)) {
            Debug::Error("Failed to create directory: " + parentPath.string() +
                         " (" + ec.message() + ")",
                         "ContentIO");
            return false;
        }
    }

    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        Debug::Error("Failed to open file for text writing", "ContentIO");
        return false;
    }

    file << text;

    if (file.fail()) {
        Debug::Error("Failed to write text file", "ContentIO");
        return false;
    }

    return true;
}

bool FileContentIO::ReadRegularFile(const fs::path& path, std::vector<std::uint8_t>& content) const {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        Debug::Error("Failed opening file for reading", "ContentIO");
        return false;
    }

    const std::streampos end = file.tellg();
    if (end < 0) {
        Debug::Error("Failed to determine file size", "ContentIO");
        return false;
    }
    if (static_cast<std::uintmax_t>(end) > content.max_size()) {
        Debug::Error("File size exceeds limit", "ContentIO");
        return false;
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!data.empty()) {
        if (!file.read(reinterpret_cast<char*>(data.data()),
                       static_cast<std::streamsize>(data.size()))) {
            Debug::Error("Failed to read file content", "ContentIO");
            return false;
        }
    }

    content = std::move(data);
    return true;
}

bool FileContentIO::WriteRegularFile(const fs::path& path, const std::vector<std::uint8_t>& content) const {
    std::filesystem::path parentPath = path.parent_path();
    if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(parentPath, ec)) {
            Debug::Error("Failed to create directory: " + parentPath.string() +
                         " (" + ec.message() + ")",
                         "ContentIO");
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        Debug::Error("Failed to open file for writing", "ContentIO");
        return false;
    }

    if (!content.empty()) {
        file.write(reinterpret_cast<const char*>(content.data()),
                   static_cast<std::streamsize>(content.size()));
    }

    if (!file.good()) {
        Debug::Error("Failed to write file content", "ContentIO");
        return false;
    }
    return true;
}