#pragma once

#include "FileType.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class FileMetaData {
public:
	using FileTime = std::filesystem::file_time_type;
	using Permissions = std::filesystem::perms;
	using SerializedData = std::vector<std::uint8_t>;

	// v2: type 由 uint32 收窄为 uint8
	// v3: FileType 取值重排，含义变化
	static constexpr std::uint32_t SerializationVersion = 3;

	FileMetaData() = default;
	FileMetaData(const FileMetaData&) = default;
	FileMetaData(FileMetaData&&) noexcept = default;
	FileMetaData& operator=(const FileMetaData&) = default;
	FileMetaData& operator=(FileMetaData&&) noexcept = default;
	~FileMetaData() = default;

	const std::string& Owner() const noexcept;
	void SetOwner(std::string owner);

	const std::string& Group() const noexcept;
	void SetGroup(std::string group);

	std::uint64_t OwnerId() const noexcept;
	void SetOwnerId(std::uint64_t id) noexcept;

	std::uint64_t GroupId() const noexcept;
	void SetGroupId(std::uint64_t id) noexcept;

	FileTime LastWriteTime() const noexcept;
	void SetLastWriteTime(FileTime time) noexcept;

	FileTime LastAccessTime() const noexcept;
	void SetLastAccessTime(FileTime time) noexcept;

	FileTime CreationTime() const noexcept;
	void SetCreationTime(FileTime time) noexcept;

	Permissions PermissionsValue() const noexcept;
	void SetPermissions(Permissions permissions) noexcept;

	FileType Type() const noexcept;
	void SetType(FileType type) noexcept;

	std::uintmax_t Size() const noexcept;
	void SetSize(std::uintmax_t size) noexcept;

	std::uintmax_t HardLinkCount() const noexcept;
	void SetHardLinkCount(std::uintmax_t count) noexcept;

	std::uint64_t DeviceId() const noexcept;
	void SetDeviceId(std::uint64_t id) noexcept;

	std::uint64_t FileId() const noexcept;
	void SetFileId(std::uint64_t id) noexcept;

	SerializedData Serialize() const;
	bool Deserialize(const SerializedData& data);

private:
	std::string owner;
	std::string group;
	std::uint64_t ownerId = 0;
	std::uint64_t groupId = 0;
	FileTime lastWriteTime{};
	FileTime lastAccessTime{};
	FileTime creationTime{};
	Permissions permissions = Permissions::unknown;
	FileType type = FileType::None;
	std::uintmax_t size = 0;
	std::uintmax_t hardLinkCount = 0;
	std::uint64_t deviceId = 0;
	std::uint64_t fileId = 0;
};
