#include "FileEntry.h"

#include "../General/SerializationUtils.h"

FileEntry::FileEntry(const std::vector<std::uint8_t>& content, const FileMetaData& metadata)
	: content(content), metadata(metadata) {
}

FileEntry::FileEntry(std::vector<std::uint8_t>&& content, FileMetaData&& metadata) noexcept
	: content(std::move(content)), metadata(std::move(metadata)) {
}

const std::vector<std::uint8_t>& FileEntry::Content() const noexcept {
	return content;
}

std::vector<std::uint8_t>& FileEntry::Content() noexcept {
	return content;
}

void FileEntry::SetContent(const std::vector<std::uint8_t>& value) {
	content = value;
}

void FileEntry::SetContent(std::vector<std::uint8_t>&& value) noexcept {
	content = std::move(value);
}

const FileMetaData& FileEntry::MetaData() const noexcept {
	return metadata;
}

FileMetaData& FileEntry::MetaData() noexcept {
	return metadata;
}

void FileEntry::SetMetaData(const FileMetaData& value) {
	metadata = value;
}

void FileEntry::SetMetaData(FileMetaData&& value) noexcept {
	metadata = std::move(value);
}

const std::filesystem::path& FileEntry::Path() const noexcept {
	return metadata.RelativePath();
}

void FileEntry::SetPath(std::filesystem::path value) {
	metadata.SetRelativePath(std::move(value));
}

FileEntry::SerializedData FileEntry::Serialize() const {
	const FileMetaData::SerializedData metadataBytes = metadata.Serialize();
	const auto& contentData = content;
	SerializedData output;
	std::size_t offset = 0;
	SerializationUtils::WriteBytes(output, offset, metadataBytes);
	SerializationUtils::WriteBytes(output, offset, contentData);
	return output;
}

bool FileEntry::Deserialize(const SerializedData& serialized) {
	std::size_t offset = 0;
	SerializedData metadataData;
	SerializedData contentData;
	if (!SerializationUtils::ReadBytes(serialized, offset, metadataData) ||
		!SerializationUtils::ReadBytes(serialized, offset, contentData) ||
		offset != serialized.size()) {
		return false;
	}

	FileEntry result;
	result.content = std::move(contentData);
	if (!result.metadata.Deserialize(metadataData)) {
		return false;
	}

	*this = std::move(result);
	return true;
}
