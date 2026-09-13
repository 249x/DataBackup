#include "FileContentIO.h"

#include <fstream>
#include <utility>

FileContentIO::FileContentIO(){

}

FileContentIO::~FileContentIO(){

}

bool FileContentIO::Read(const std::filesystem::path& path, std::vector<std::uint8_t>& content) const {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		return false;
	}

	const std::streampos end = file.tellg();
	if (end < 0 || static_cast<std::uintmax_t>(end) > content.max_size()) {
		return false;
	}

	std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
	file.seekg(0, std::ios::beg);
	if (!data.empty() && !file.read(reinterpret_cast<char*>(data.data()),
	                                static_cast<std::streamsize>(data.size()))) {
		return false;
	}

	content = std::move(data);
	return true;
}

bool FileContentIO::Write(const std::filesystem::path& path, const std::vector<std::uint8_t>& content) const {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		return false;
	}

	const std::vector<std::uint8_t>& data = content;
	if (!data.empty()) {
		file.write(reinterpret_cast<const char*>(data.data()),
		           static_cast<std::streamsize>(data.size()));
	}

	return file.good();
}

bool FileContentIO::ReadText(const std::filesystem::path& filePath, std::string& text) const {
    if (!std::filesystem::exists(filePath)) {
        return false;
    }

    if (!std::filesystem::is_regular_file(filePath)) {
        return false;
    }
    
        std::ifstream file(filePath, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        std::ostringstream oss;
        oss << file.rdbuf();
        text = oss.str();
        
        file.close();
        return true;
}

bool FileContentIO::WriteText(const std::filesystem::path& filePath, const std::string& text) const {
    std::filesystem::path parentPath = filePath.parent_path();
    if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
        std::filesystem::create_directories(parentPath);
    }
        
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
        
    file << text;
    file.close();
    
    return true;
}