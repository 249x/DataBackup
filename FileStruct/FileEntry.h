#pragma once

#include "FileMetaData.h"

#include <filesystem>
#include <cstdint>
#include <utility>
#include <vector>

class FileEntry {
public:
	using SerializedData = std::vector<std::uint8_t>;

	FileEntry() = default;
	FileEntry(const std::vector<std::uint8_t>& content, const FileMetaData& metadata);
	FileEntry(std::vector<std::uint8_t>&& content, FileMetaData&& metadata) noexcept;

	FileEntry(const FileEntry&) = default;
	FileEntry(FileEntry&&) noexcept = default;
	FileEntry& operator=(const FileEntry&) = default;
	FileEntry& operator=(FileEntry&&) noexcept = default;
	~FileEntry() = default;

	const std::vector<std::uint8_t>& Content() const noexcept;
	std::vector<std::uint8_t>& Content() noexcept;
	void SetContent(const std::vector<std::uint8_t>& value);
	void SetContent(std::vector<std::uint8_t>&& value) noexcept;

	const FileMetaData& MetaData() const noexcept;
	FileMetaData& MetaData() noexcept;
	void SetMetaData(const FileMetaData& value);
	void SetMetaData(FileMetaData&& value) noexcept;

	const std::filesystem::path& Path() const noexcept;
	void SetPath(std::filesystem::path value);

	SerializedData Serialize() const;
	bool Deserialize(const SerializedData& data);

private:
	std::vector<std::uint8_t> content;
	FileMetaData metadata;
	std::filesystem::path path;
};
