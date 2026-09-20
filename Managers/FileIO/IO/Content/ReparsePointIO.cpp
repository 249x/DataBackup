#include "ReparsePointIO.h"

#include <cstddef>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "../../../../General/Debug.h"

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

#ifndef IO_REPARSE_TAG_AF_UNIX
#define IO_REPARSE_TAG_AF_UNIX 0x80000023
#endif

// MinGW 的 winnt.h 不提供 REPARSE_DATA_BUFFER（它属于 WDK 的 ntifs.h），
// 这里按 Windows 官方内存布局自行声明。
struct SymbolicLinkBuffer {
	USHORT SubstituteNameOffset;
	USHORT SubstituteNameLength;
	USHORT PrintNameOffset;
	USHORT PrintNameLength;
	ULONG Flags;
	WCHAR PathBuffer[1];
};

struct MountPointBuffer {
	USHORT SubstituteNameOffset;
	USHORT SubstituteNameLength;
	USHORT PrintNameOffset;
	USHORT PrintNameLength;
	WCHAR PathBuffer[1];
};

struct ReparseBuffer {
	ULONG ReparseTag;
	USHORT ReparseDataLength;
	USHORT Reserved;
	union {
		SymbolicLinkBuffer SymbolicLink;
		MountPointBuffer MountPoint;
		UCHAR Generic[1];
	};
};

// 以"打开链接本体"的方式获取句柄，避免被跟随到目标
HANDLE OpenReparsePoint(const fs::path& path, DWORD access) {
	const std::wstring native = path.wstring();
	return CreateFileW(native.c_str(), access,
	                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
	                   OPEN_EXISTING,
	                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}

// 链接目标在重解析缓冲区内是 NT 路径（\??\C:\... 或 \??\UNC\...），去掉前缀
std::wstring NormalizeTargetPath(const std::wstring& target) {
	const std::wstring uncPrefix = L"\\??\\UNC\\";
	if (target.compare(0, uncPrefix.size(), uncPrefix) == 0) {
		return L"\\\\" + target.substr(uncPrefix.size());
	}
	const std::wstring ntPrefix = L"\\??\\";
	if (target.compare(0, ntPrefix.size(), ntPrefix) == 0) {
		return target.substr(ntPrefix.size());
	}
	return target;
}

// junction 不需要特权，作为无法创建符号链接时的回退方案（仅支持绝对目录目标）
bool CreateJunction(const fs::path& link, const fs::path& target) {
	std::error_code error;
	const fs::path absolute = fs::absolute(target, error);
	if (error || !fs::is_directory(absolute, error)) {
		return false;
	}

	const std::wstring linkNative = link.wstring();
	if (CreateDirectoryW(linkNative.c_str(), nullptr) == FALSE) {
		return false;
	}

	const HANDLE handle = OpenReparsePoint(link, GENERIC_WRITE);
	if (handle == INVALID_HANDLE_VALUE) {
		RemoveDirectoryW(linkNative.c_str());
		return false;
	}

	// 布局必须与 Windows 自身（mklink /J）写出的完全一致：
	// SubstituteName 为 NT 路径，之后隔一个空字符放非空的 PrintName，
	// 两个长度都不含各自的结尾空字符。PrintNameLength 为 0、或偏移对不上，
	// 都会被 FSCTL_SET_REPARSE_POINT 以 ERROR_REPARSE_TAG_INVALID(4392) 拒绝。
	const std::wstring substituteName = L"\\??\\" + absolute.wstring();
	const std::wstring printName = absolute.wstring();
	const USHORT substituteBytes =
		static_cast<USHORT>(substituteName.size() * sizeof(wchar_t));
	const USHORT printBytes = static_cast<USHORT>(printName.size() * sizeof(wchar_t));
	const USHORT nullBytes = static_cast<USHORT>(sizeof(wchar_t));

	const USHORT dataLength = static_cast<USHORT>(
		offsetof(MountPointBuffer, PathBuffer) + substituteBytes + nullBytes + printBytes +
		nullBytes);

	std::vector<std::uint8_t> buffer(offsetof(ReparseBuffer, MountPoint) + dataLength, 0);
	auto* reparse = reinterpret_cast<ReparseBuffer*>(buffer.data());
	reparse->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
	reparse->ReparseDataLength = dataLength;
	reparse->MountPoint.SubstituteNameOffset = 0;
	reparse->MountPoint.SubstituteNameLength = substituteBytes;
	reparse->MountPoint.PrintNameOffset = substituteBytes + nullBytes;
	reparse->MountPoint.PrintNameLength = printBytes;

	std::uint8_t* pathBuffer = buffer.data() + offsetof(ReparseBuffer, MountPoint) +
	                           offsetof(MountPointBuffer, PathBuffer);
	std::memcpy(pathBuffer, substituteName.data(), substituteBytes);
	std::memcpy(pathBuffer + reparse->MountPoint.PrintNameOffset, printName.data(),
	            printBytes);

	DWORD returned = 0;
	const BOOL written = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(),
	                                     static_cast<DWORD>(buffer.size()), nullptr, 0,
	                                     &returned, nullptr);
	if (!written) {
		Debug::Error("Failed to set junction reparse point: " + link.string() +
		                 " (win32 error " + std::to_string(GetLastError()) + ")",
		             "ReparsePoint");
	}
	CloseHandle(handle);
	if (!written) {
		RemoveDirectoryW(linkNative.c_str());
		return false;
	}
	return true;
}

bool ReadTargetImpl(const fs::path& path, std::vector<std::uint8_t>& content) {
	const HANDLE handle = OpenReparsePoint(path, FILE_READ_ATTRIBUTES);
	if (handle == INVALID_HANDLE_VALUE) {
		Debug::Error("Failed to open reparse point: " + path.string(), "ReparsePoint");
		return false;
	}

	std::vector<std::uint8_t> buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
	DWORD returned = 0;
	const BOOL read = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0,
	                                  buffer.data(), static_cast<DWORD>(buffer.size()),
	                                  &returned, nullptr);
	CloseHandle(handle);
	if (!read) {
		Debug::Error("Failed to read reparse point: " + path.string(), "ReparsePoint");
		return false;
	}

	const auto* reparse = reinterpret_cast<const ReparseBuffer*>(buffer.data());
	const wchar_t* target = nullptr;
	USHORT targetBytes = 0;
	if (reparse->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
		target = reinterpret_cast<const wchar_t*>(
			buffer.data() + offsetof(ReparseBuffer, SymbolicLink) +
			offsetof(SymbolicLinkBuffer, PathBuffer) +
			reparse->SymbolicLink.SubstituteNameOffset);
		targetBytes = reparse->SymbolicLink.SubstituteNameLength;
	} else if (reparse->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
		target = reinterpret_cast<const wchar_t*>(
			buffer.data() + offsetof(ReparseBuffer, MountPoint) +
			offsetof(MountPointBuffer, PathBuffer) +
			reparse->MountPoint.SubstituteNameOffset);
		targetBytes = reparse->MountPoint.SubstituteNameLength;
	}

	if (target == nullptr || targetBytes == 0) {
		Debug::Error("Reparse point has no target: " + path.string(), "ReparsePoint");
		return false;
	}

	const std::wstring targetPath =
		NormalizeTargetPath(std::wstring(target, targetBytes / sizeof(wchar_t)));
	const std::string utf8 = fs::path(targetPath).u8string();
	content.assign(utf8.begin(), utf8.end());
	return true;
}

bool CreateImpl(const fs::path& link, const fs::path& target) {
	const std::wstring linkNative = link.wstring();
	const std::wstring targetNative = target.wstring();

	// 目标已存在时先移除，保证还原可以重复执行
	if (GetFileAttributesW(linkNative.c_str()) != INVALID_FILE_ATTRIBUTES) {
		DeleteFileW(linkNative.c_str());
		RemoveDirectoryW(linkNative.c_str());
	}

	DWORD linkFlags = 0;
	const DWORD targetAttributes = GetFileAttributesW(targetNative.c_str());
	if (targetAttributes != INVALID_FILE_ATTRIBUTES &&
	    (targetAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		linkFlags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
	}

	DWORD lastError = ERROR_SUCCESS;
	auto attempt = [&](DWORD flags) {
		if (CreateSymbolicLinkW(linkNative.c_str(), targetNative.c_str(), flags) != FALSE) {
			return true;
		}
		lastError = GetLastError();
		return false;
	};

	if (attempt(linkFlags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
		return true;
	}
	if (lastError == ERROR_INVALID_PARAMETER && attempt(linkFlags)) {
		// 旧版 Windows 不识别 ALLOW_UNPRIVILEGED_CREATE
		return true;
	}
	if (lastError == ERROR_PRIVILEGE_NOT_HELD && CreateJunction(link, target)) {
		return true;
	}

	Debug::Error("Failed to create link: " + link.string() + " -> " + target.string() +
	                 " (win32 error " + std::to_string(lastError) + ")",
	             "ReparsePoint");
	return false;
}

bool CreateSocketImpl(const fs::path& path) {
	const std::wstring native = path.wstring();

	// 目标已存在时先移除，保证还原可以重复执行
	if (GetFileAttributesW(native.c_str()) != INVALID_FILE_ATTRIBUTES) {
		DeleteFileW(native.c_str());
		RemoveDirectoryW(native.c_str());
	}

	// 必须先建出空文件再打标记，Windows 才会把它当成套接字文件
	const HANDLE created = CreateFileW(native.c_str(), GENERIC_WRITE, 0, nullptr,
	                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (created == INVALID_HANDLE_VALUE) {
		Debug::Error("Failed to create socket file: " + path.string() + " (win32 error " +
		                 std::to_string(GetLastError()) + ")",
		             "ReparsePoint");
		return false;
	}
	CloseHandle(created);

	const HANDLE handle = OpenReparsePoint(path, GENERIC_WRITE);
	if (handle == INVALID_HANDLE_VALUE) {
		Debug::Error("Failed to open socket file: " + path.string() + " (win32 error " +
		                 std::to_string(GetLastError()) + ")",
		             "ReparsePoint");
		DeleteFileW(native.c_str());
		return false;
	}

	// 实测：真实的 AF_UNIX 套接字就是该标记 + 空数据
	struct SocketBuffer {
		ULONG ReparseTag;
		USHORT ReparseDataLength;
		USHORT Reserved;
	} buffer{IO_REPARSE_TAG_AF_UNIX, 0, 0};

	DWORD returned = 0;
	const BOOL written = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, &buffer,
	                                     static_cast<DWORD>(sizeof(buffer)), nullptr, 0,
	                                     &returned, nullptr);
	const DWORD error = GetLastError();
	CloseHandle(handle);
	if (!written) {
		Debug::Error("Failed to set socket reparse point: " + path.string() +
		                 " (win32 error " + std::to_string(error) + ")",
		             "ReparsePoint");
		DeleteFileW(native.c_str());
		return false;
	}
	return true;
}

#else

bool ReadTargetImpl(const fs::path& path, std::vector<std::uint8_t>& content) {
	std::error_code error;
	const fs::path target = fs::read_symlink(path, error);
	if (error) {
		Debug::Error("Failed to read symlink: " + path.string() + " (" + error.message() + ")",
		             "ReparsePoint");
		return false;
	}
	const std::string utf8 = target.u8string();
	content.assign(utf8.begin(), utf8.end());
	return true;
}

bool CreateImpl(const fs::path& link, const fs::path& target) {
	std::error_code error;
	fs::remove(link, error);
	error.clear();
	if (fs::is_directory(target, error)) {
		fs::create_directory_symlink(target, link, error);
	} else {
		fs::create_symlink(target, link, error);
	}
	if (error) {
		Debug::Error("Failed to create link: " + link.string() + " -> " + target.string() +
		                 " (" + error.message() + ")",
		             "ReparsePoint");
		return false;
	}
	return true;
}

bool CreateSocketImpl(const fs::path& path) {
	Debug::Error("AF_UNIX sockets are not supported on this platform: " + path.string(),
	             "ReparsePoint");
	return false;
}

#endif // _WIN32

} // namespace

bool ReparsePointIO::ReadTarget(const fs::path& path, std::vector<std::uint8_t>& content) {
	return ReadTargetImpl(path, content);
}

bool ReparsePointIO::Create(const fs::path& link, const fs::path& target) {
	return CreateImpl(link, target);
}

bool ReparsePointIO::CreateSocket(const fs::path& path) {
	return CreateSocketImpl(path);
}
