#include "WindowsFileMetaDataIO.h"

#ifdef _WIN32

#include <windows.h>
#include <aclapi.h>

#include <chrono>
#include <limits>
#include <string>
#include <vector>

namespace {

std::wstring ToWide(const std::filesystem::path& path) {
	return path.wstring();
}

FileMetaData::FileTime ToFileTime(const FILETIME& value) {
	ULARGE_INTEGER ticks;
	ticks.LowPart = value.dwLowDateTime;
	ticks.HighPart = value.dwHighDateTime;

	using Duration = std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>;
	const auto systemTime = std::chrono::system_clock::time_point(Duration(
		static_cast<std::int64_t>(ticks.QuadPart) - 116444736000000000LL));
	return FileMetaData::FileTime::clock::now() +
		(systemTime - std::chrono::system_clock::now());
}

FILETIME ToWindowsFileTime(FileMetaData::FileTime value) {
	const auto systemTime = std::chrono::system_clock::now() +
		(value - FileMetaData::FileTime::clock::now());
	const auto duration = systemTime.time_since_epoch();
	const auto ticks = std::chrono::duration_cast<
		std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>>(duration).count() +
		116444736000000000LL;

	ULARGE_INTEGER result;
	result.QuadPart = static_cast<unsigned long long>(ticks);
	FILETIME fileTime;
	fileTime.dwLowDateTime = result.LowPart;
	fileTime.dwHighDateTime = result.HighPart;
	return fileTime;
}

std::string AccountName(PSID sid) {
	if (sid == nullptr || !IsValidSid(sid)) {
		return {};
	}

	DWORD nameSize = 0;
	DWORD domainSize = 0;
	SID_NAME_USE use;
	LookupAccountSidW(nullptr, sid, nullptr, &nameSize, nullptr, &domainSize, &use);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		return {};
	}

	std::vector<wchar_t> name(nameSize);
	std::vector<wchar_t> domain(domainSize);
	if (!LookupAccountSidW(nullptr, sid, name.data(), &nameSize, domain.data(),
	                       &domainSize, &use)) {
		return {};
	}

	return std::filesystem::path(name.data()).string();
}

std::uint64_t AccountId(PSID sid) {
	if (sid == nullptr || !IsValidSid(sid)) {
		return 0;
	}
	const auto count = *GetSidSubAuthorityCount(sid);
	return count == 0 ? 0 : *GetSidSubAuthority(sid, count - 1);
}

bool ReadSecurity(const std::wstring& path, FileMetaData& metadata) {
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	PSID owner = nullptr;
	PSID group = nullptr;
	const DWORD result = GetNamedSecurityInfoW(
		path.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
		&owner, &group, nullptr, nullptr, &descriptor);
	if (result != ERROR_SUCCESS) {
		return false;
	}

	metadata.SetOwner(AccountName(owner));
	metadata.SetOwnerId(AccountId(owner));
	metadata.SetGroup(AccountName(group));
	metadata.SetGroupId(AccountId(group));
	LocalFree(descriptor);
	return true;
}

} // namespace

bool WindowsFileMetaDataIO::Read(const std::filesystem::path& path, FileMetaData& metadata) const {
	const std::wstring nativePath = ToWide(path);
	const DWORD attributes = GetFileAttributesW(nativePath.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return false;
	}

	const HANDLE handle = CreateFileW(nativePath.c_str(), 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	BY_HANDLE_FILE_INFORMATION information;
	const bool read = GetFileInformationByHandle(handle, &information) != FALSE;
	if (read) {
		metadata.SetCreationTime(ToFileTime(information.ftCreationTime));
		metadata.SetLastAccessTime(ToFileTime(information.ftLastAccessTime));
		metadata.SetLastWriteTime(ToFileTime(information.ftLastWriteTime));
		metadata.SetSize((static_cast<std::uintmax_t>(information.nFileSizeHigh) << 32) |
			information.nFileSizeLow);
		metadata.SetHardLinkCount(information.nNumberOfLinks);
		metadata.SetDeviceId(information.dwVolumeSerialNumber);
		metadata.SetFileId((static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) |
			information.nFileIndexLow);
		metadata.SetType((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0
			? FileMetaData::FileType::directory : FileMetaData::FileType::regular);
		metadata.SetPermissions((attributes & FILE_ATTRIBUTE_READONLY) != 0
			? (FileMetaData::Permissions::owner_read |
			   FileMetaData::Permissions::group_read |
			   FileMetaData::Permissions::others_read)
			: FileMetaData::Permissions::owner_read |
			  FileMetaData::Permissions::owner_write |
			  FileMetaData::Permissions::group_read |
			  FileMetaData::Permissions::others_read);
		ReadSecurity(nativePath, metadata);
	}

	CloseHandle(handle);
	return read;
}

bool WindowsFileMetaDataIO::Write(const std::filesystem::path& path, const FileMetaData& metadata) const {
	const std::wstring nativePath = ToWide(path);
	const HANDLE handle = CreateFileW(nativePath.c_str(), FILE_WRITE_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	const FILETIME creation = ToWindowsFileTime(metadata.CreationTime());
	const FILETIME access = ToWindowsFileTime(metadata.LastAccessTime());
	const FILETIME write = ToWindowsFileTime(metadata.LastWriteTime());
	const bool timesWritten = SetFileTime(handle, &creation, &access, &write) != FALSE;
	CloseHandle(handle);

	DWORD attributes = GetFileAttributesW(nativePath.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	const bool writable = (metadata.PermissionsValue() &
		FileMetaData::Permissions::owner_write) != FileMetaData::Permissions::none;
	if (writable) {
		attributes &= ~FILE_ATTRIBUTE_READONLY;
	} else {
		attributes |= FILE_ATTRIBUTE_READONLY;
	}
	return timesWritten && SetFileAttributesW(nativePath.c_str(), attributes) != FALSE;
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
