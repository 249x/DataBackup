#include "FileMetaData.h"

#include "../General/SerializationUtils.h"
#include "../General/Utf8.h"

#include <limits>
#include <utility>

const std::filesystem::path& FileMetaData::RelativePath() const noexcept {
	return relativePath;
}

void FileMetaData::SetRelativePath(std::filesystem::path value) {
	relativePath = std::move(value);
}

const std::string& FileMetaData::Owner() const noexcept {
	return owner;
}

void FileMetaData::SetOwner(std::string value) {
	owner = std::move(value);
}

const std::string& FileMetaData::Group() const noexcept {
	return group;
}

void FileMetaData::SetGroup(std::string value) {
	group = std::move(value);
}

std::uint64_t FileMetaData::OwnerId() const noexcept {
	return ownerId;
}

void FileMetaData::SetOwnerId(std::uint64_t id) noexcept {
	ownerId = id;
}

std::uint64_t FileMetaData::GroupId() const noexcept {
	return groupId;
}

void FileMetaData::SetGroupId(std::uint64_t id) noexcept {
	groupId = id;
}

FileMetaData::FileTime FileMetaData::LastWriteTime() const noexcept {
	return lastWriteTime;
}

void FileMetaData::SetLastWriteTime(FileTime time) noexcept {
	lastWriteTime = time;
}

FileMetaData::FileTime FileMetaData::LastAccessTime() const noexcept {
	return lastAccessTime;
}

void FileMetaData::SetLastAccessTime(FileTime time) noexcept {
	lastAccessTime = time;
}

FileMetaData::FileTime FileMetaData::CreationTime() const noexcept {
	return creationTime;
}

void FileMetaData::SetCreationTime(FileTime time) noexcept {
	creationTime = time;
}

FileMetaData::Permissions FileMetaData::PermissionsValue() const noexcept {
	return permissions;
}

void FileMetaData::SetPermissions(Permissions value) noexcept {
	permissions = value;
}

FileType FileMetaData::Type() const noexcept {
	return type;
}

void FileMetaData::SetType(FileType value) noexcept {
	type = value;
}

std::uintmax_t FileMetaData::Size() const noexcept {
	return size;
}

void FileMetaData::SetSize(std::uintmax_t value) noexcept {
	size = value;
}

std::uintmax_t FileMetaData::HardLinkCount() const noexcept {
	return hardLinkCount;
}

void FileMetaData::SetHardLinkCount(std::uintmax_t value) noexcept {
	hardLinkCount = value;
}

std::uint64_t FileMetaData::DeviceId() const noexcept {
	return deviceId;
}

void FileMetaData::SetDeviceId(std::uint64_t id) noexcept {
	deviceId = id;
}

std::uint64_t FileMetaData::FileId() const noexcept {
	return fileId;
}

void FileMetaData::SetFileId(std::uint64_t id) noexcept {
	fileId = id;
}

FileMetaData::SerializedData FileMetaData::Serialize() const {
	SerializedData output;
	std::size_t offset = 0;

	SerializationUtils::WriteUnsigned<std::uint32_t>(output, offset, SerializationVersion);
	SerializationUtils::WriteString(output, offset, Utf8::FromPath(relativePath));
	SerializationUtils::WriteString(output, offset, owner);
	SerializationUtils::WriteString(output, offset, group);
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset, ownerId);
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset, groupId);
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset,
		static_cast<std::uint64_t>(lastWriteTime.time_since_epoch().count()));
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset,
		static_cast<std::uint64_t>(lastAccessTime.time_since_epoch().count()));
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset,
		static_cast<std::uint64_t>(creationTime.time_since_epoch().count()));
	SerializationUtils::WriteUnsigned<std::uint32_t>(output, offset,
		static_cast<std::uint32_t>(permissions));
	SerializationUtils::WriteUnsigned<std::uint8_t>(output, offset,
		static_cast<std::uint8_t>(type));
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset, static_cast<std::uint64_t>(size));
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset, static_cast<std::uint64_t>(hardLinkCount));
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset, deviceId);
	SerializationUtils::WriteUnsigned<std::uint64_t>(output, offset, fileId);
	return output;
}

bool FileMetaData::Deserialize(const SerializedData& serialized) {
	std::size_t offset = 0;
	std::uint32_t version = 0;
	if (!SerializationUtils::ReadUnsigned(serialized, offset, version) ||
		version != SerializationVersion) {
		return false;
	}

	FileMetaData result;
	std::string relativePath;
	if (!SerializationUtils::ReadString(serialized, offset, relativePath) ||
		!SerializationUtils::ReadString(serialized, offset, result.owner) ||
		!SerializationUtils::ReadString(serialized, offset, result.group) ||
		!SerializationUtils::ReadUnsigned(serialized, offset, result.ownerId) ||
		!SerializationUtils::ReadUnsigned(serialized, offset, result.groupId)) {
		return false;
	}

	std::uint64_t timeValue = 0;
	if (!SerializationUtils::ReadUnsigned(serialized, offset, timeValue)) {
		return false;
	}
	result.lastWriteTime = FileTime(FileTime::duration(
		static_cast<FileTime::duration::rep>(timeValue)));
	if (!SerializationUtils::ReadUnsigned(serialized, offset, timeValue)) {
		return false;
	}
	result.lastAccessTime = FileTime(FileTime::duration(
		static_cast<FileTime::duration::rep>(timeValue)));
	if (!SerializationUtils::ReadUnsigned(serialized, offset, timeValue)) {
		return false;
	}
	result.creationTime = FileTime(FileTime::duration(
		static_cast<FileTime::duration::rep>(timeValue)));

	std::uint32_t enumValue = 0;
	if (!SerializationUtils::ReadUnsigned(serialized, offset, enumValue)) {
		return false;
	}
	result.permissions = static_cast<Permissions>(enumValue);

	std::uint8_t typeValue = 0;
	if (!SerializationUtils::ReadUnsigned(serialized, offset, typeValue) ||
		!IsValidFileTypeValue(typeValue)) {
		return false;
	}
	result.type = static_cast<FileType>(typeValue);

	if (!SerializationUtils::ReadUnsigned(serialized, offset, result.size) ||
		!SerializationUtils::ReadUnsigned(serialized, offset, result.hardLinkCount) ||
		!SerializationUtils::ReadUnsigned(serialized, offset, result.deviceId) ||
		!SerializationUtils::ReadUnsigned(serialized, offset, result.fileId) ||
		offset != serialized.size()) {
		return false;
	}

	result.relativePath = Utf8::ToPath(relativePath);
	*this = std::move(result);
	return true;
}
