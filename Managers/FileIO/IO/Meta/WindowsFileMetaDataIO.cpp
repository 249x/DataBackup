#include "WindowsFileMetaDataIO.h"

#ifdef _WIN32

#include "WindowsMetaDataUtils.h"

#include <windows.h>
#include <string>

#include "../../../../General/Debug.h"

namespace {

constexpr const char* LogTag = WindowsMetaDataUtils::LogTag;

} // namespace

bool WindowsFileMetaDataIO::Read(const std::filesystem::path& path, FileMetaData& metadata) const {
	const std::wstring nativePath = WindowsMetaDataUtils::ToWide(path);

	// 先确认路径存在再读属性。GetFileAttributesW 不跟随重解析点，所以这里判断的是
	// "链接本体"是否存在——悬空链接同样算存在，与后面用 FILE_FLAG_OPEN_REPARSE_POINT
	// 打开保持一致；因此不能用 std::filesystem::exists（它会跟随链接而把悬空链接判为不存在）。
	const DWORD attributes = GetFileAttributesW(nativePath.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		const DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
			Debug::Error("Path does not exist: " + path.string(), LogTag);
		} else if (error == ERROR_ACCESS_DENIED) {
			Debug::Error("Access denied: " + path.string(), LogTag);
		} else {
			Debug::Error("Failed to query attributes: " + path.string() +
			                 " (win32 error " + std::to_string(error) + ")",
			             LogTag);
		}
		return false;
	}

	// FILE_FLAG_OPEN_REPARSE_POINT：对链接读取链接本体而非其目标
	// FILE_READ_ATTRIBUTES：读取重解析标记（FileAttributeTagInfo）所需
	const HANDLE handle = CreateFileW(nativePath.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		Debug::Error("Failed to open for reading metadata", LogTag);
		return false;
	}

	BY_HANDLE_FILE_INFORMATION information;
	if (GetFileInformationByHandle(handle, &information) == FALSE) {
		Debug::Error("Failed to read file information: ", LogTag);
		CloseHandle(handle);
		return false;
	}

	metadata.SetCreationTime(WindowsMetaDataUtils::ToFileTime(information.ftCreationTime));
	metadata.SetLastAccessTime(WindowsMetaDataUtils::ToFileTime(information.ftLastAccessTime));
	metadata.SetLastWriteTime(WindowsMetaDataUtils::ToFileTime(information.ftLastWriteTime));
	metadata.SetSize((static_cast<std::uintmax_t>(information.nFileSizeHigh) << 32) |
		information.nFileSizeLow);
	metadata.SetHardLinkCount(information.nNumberOfLinks);
	metadata.SetDeviceId(information.dwVolumeSerialNumber);
	metadata.SetFileId((static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
		information.nFileIndexLow);

	FileType type = WindowsMetaDataUtils::DetectType(path, attributes, handle);
	if (type == FileType::Regular && information.nNumberOfLinks > 1) {
		type = FileType::HardLink;
	}
	metadata.SetType(type);

	metadata.SetPermissions((attributes & FILE_ATTRIBUTE_READONLY) != 0
		? (FileMetaData::Permissions::owner_read |
		   FileMetaData::Permissions::group_read |
		   FileMetaData::Permissions::others_read)
		: FileMetaData::Permissions::owner_read |
		  FileMetaData::Permissions::owner_write |
		  FileMetaData::Permissions::group_read |
		  FileMetaData::Permissions::others_read);

	if (!WindowsMetaDataUtils::ReadSecurity(nativePath, metadata)) {
		Debug::Warning("Failed to read owner/group", LogTag);
	}

	CloseHandle(handle);
	return true;
}

bool WindowsFileMetaDataIO::Write(const std::filesystem::path& path, const FileMetaData& metadata) const {
	const FileType type = metadata.Type();
	// 只有能承载内容的类型与目录才写回元数据
	const bool restorable = type == FileType::None || IsFileContentType(type) ||
	                        type == FileType::Directory;
	if (!restorable) {
		return true;
	}

	const std::wstring nativePath = WindowsMetaDataUtils::ToWide(path);
	const HANDLE handle = CreateFileW(nativePath.c_str(), FILE_WRITE_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		Debug::Error("Failed to open for writing metadata", LogTag);
		return false;
	}

	const FILETIME creation = WindowsMetaDataUtils::ToWindowsFileTime(metadata.CreationTime());
	const FILETIME access = WindowsMetaDataUtils::ToWindowsFileTime(metadata.LastAccessTime());
	const FILETIME write = WindowsMetaDataUtils::ToWindowsFileTime(metadata.LastWriteTime());
	const bool timesWritten = SetFileTime(handle, &creation, &access, &write) != FALSE;

	CloseHandle(handle);
	if (!timesWritten) {
		Debug::Warning("Failed to restore timestamps", LogTag);
	}

	DWORD attributes = GetFileAttributesW(nativePath.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		Debug::Error("Failed to query attributes", LogTag);
		return false;
	}
	const bool writable = (metadata.PermissionsValue() &
		FileMetaData::Permissions::owner_write) != FileMetaData::Permissions::none;
	if (writable) {
		attributes &= ~FILE_ATTRIBUTE_READONLY;
	} else {
		attributes |= FILE_ATTRIBUTE_READONLY;
	}
	if (SetFileAttributesW(nativePath.c_str(), attributes) == FALSE) {
		Debug::Warning("Failed to restore attributes", LogTag);
		return false;
	}
	return timesWritten;
}

#else

bool WindowsFileMetaDataIO::Read(const std::filesystem::path&,
	                               FileMetaData&) const {
	return false;
}

bool WindowsFileMetaDataIO::Write(const std::filesystem::path&,
	                                const FileMetaData&) const {
	return false;
}

#endif
