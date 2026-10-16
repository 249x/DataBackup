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
	SerializationUtils io;

	io.WriteUnsigned<std::uint32_t>(output, SerializationVersion);
	io.WriteString(output, Utf8::FromPath(relativePath));
	io.WriteString(output, owner);
	io.WriteString(output, group);
	io.WriteUnsigned<std::uint64_t>(output, ownerId);
	io.WriteUnsigned<std::uint64_t>(output, groupId);
	io.WriteUnsigned<std::uint64_t>(output,
		static_cast<std::uint64_t>(lastWriteTime.time_since_epoch().count()));
	io.WriteUnsigned<std::uint64_t>(output,
		static_cast<std::uint64_t>(lastAccessTime.time_since_epoch().count()));
	io.WriteUnsigned<std::uint64_t>(output,
		static_cast<std::uint64_t>(creationTime.time_since_epoch().count()));
	io.WriteUnsigned<std::uint32_t>(output,
		static_cast<std::uint32_t>(permissions));
	io.WriteUnsigned<std::uint8_t>(output,
		static_cast<std::uint8_t>(type));
	io.WriteUnsigned<std::uint64_t>(output, static_cast<std::uint64_t>(size));
	io.WriteUnsigned<std::uint64_t>(output, static_cast<std::uint64_t>(hardLinkCount));
	io.WriteUnsigned<std::uint64_t>(output, deviceId);
	io.WriteUnsigned<std::uint64_t>(output, fileId);
	return output;
}

bool FileMetaData::Deserialize(const SerializedData& serialized) {
	SerializationUtils io;
	std::uint32_t version = 0;
	if (!io.ReadUnsigned(serialized, version) ||
		version != SerializationVersion) {
		return false;
	}

	FileMetaData result;
	std::string relativePath;
	if (!io.ReadString(serialized, relativePath) ||
		!io.ReadString(serialized, result.owner) ||
		!io.ReadString(serialized, result.group) ||
		!io.ReadUnsigned(serialized, result.ownerId) ||
		!io.ReadUnsigned(serialized, result.groupId)) {
		return false;
	}

	std::uint64_t timeValue = 0;
	if (!io.ReadUnsigned(serialized, timeValue)) {
		return false;
	}
	result.lastWriteTime = FileTime(FileTime::duration(
		static_cast<FileTime::duration::rep>(timeValue)));
	if (!io.ReadUnsigned(serialized, timeValue)) {
		return false;
	}
	result.lastAccessTime = FileTime(FileTime::duration(
		static_cast<FileTime::duration::rep>(timeValue)));
	if (!io.ReadUnsigned(serialized, timeValue)) {
		return false;
	}
	result.creationTime = FileTime(FileTime::duration(
		static_cast<FileTime::duration::rep>(timeValue)));

	std::uint32_t enumValue = 0;
	if (!io.ReadUnsigned(serialized, enumValue)) {
		return false;
	}
	result.permissions = static_cast<Permissions>(enumValue);

	std::uint8_t typeValue = 0;
	if (!io.ReadUnsigned(serialized, typeValue) ||
		!IsValidFileTypeValue(typeValue)) {
		return false;
	}
	result.type = static_cast<FileType>(typeValue);

	if (!io.ReadUnsigned(serialized, result.size) ||
		!io.ReadUnsigned(serialized, result.hardLinkCount) ||
		!io.ReadUnsigned(serialized, result.deviceId) ||
		!io.ReadUnsigned(serialized, result.fileId) ||
		io.Offset() != serialized.size()) {
		return false;
	}

	result.relativePath = Utf8::ToPath(relativePath);
	*this = std::move(result);
	return true;
}
