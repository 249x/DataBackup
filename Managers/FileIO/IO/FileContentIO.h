#pragma once

#include <vector>
#include <filesystem>

class FileContentIO {
public:
	FileContentIO();
	~FileContentIO();

	bool Read(const std::filesystem::path& path, std::vector<std::uint8_t>& content) const;
	bool Write(const std::filesystem::path& path, const std::vector<std::uint8_t>& content) const;
	bool ReadText(const std::filesystem::path& filePath, std::string& text) const;
    bool WriteText(const std::filesystem::path& filePath, const std::string& text) const;
};
