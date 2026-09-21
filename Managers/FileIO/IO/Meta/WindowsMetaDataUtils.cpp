#include "WindowsMetaDataUtils.h"

#include <aclapi.h>

#include <chrono>
#include <limits>
#include <string>
#include <vector>

#include "../../../../General/Debug.h"
#include "../../../../General/Utf8.h"

namespace {

// 本文件的日志标签，与类里对外暴露的是同一个来源
constexpr const char* LogTag = WindowsMetaDataUtils::LogTag;

using HundredNanoseconds = std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>;

// FILETIME 的起点 1601-01-01 到 Unix 纪元 1970-01-01 之间的百纳秒数
constexpr std::int64_t WindowsEpochToUnixTicks = 116444736000000000LL;

/*
 * 两个时钟的一次性采样基准对。
 *
 * 把 system_clock 的时刻映射到 file_clock 需要知道二者纪元之差，而标准并未规定
 * file_clock 的纪元，只能靠"同一时刻各采一次"来推算。基准只采一次并长期复用，
 * 这样两个方向的换算互为精确逆运算
 */
struct ClockBases {
	std::chrono::system_clock::time_point system;
	FileMetaData::FileTime file;
	std::int64_t fileTicks;
};

const ClockBases& GetClockBases() {
	static const ClockBases bases = [] {
		const auto system = std::chrono::system_clock::now();
		const auto file = FileMetaData::FileTime::clock::now();
		return ClockBases{system, file, file.time_since_epoch().count()};
	}();
	return bases;
}

// file_clock 的可表示范围有限（本工具链是纳秒，只覆盖约 1882-2466 年），
FileMetaData::FileTime SaturateToFileTime(__int128 ticks) {
	const auto minimum = FileMetaData::FileTime::min().time_since_epoch().count();
	const auto maximum = FileMetaData::FileTime::max().time_since_epoch().count();
	if (ticks >= minimum && ticks <= maximum) {
		return FileMetaData::FileTime(FileMetaData::FileTime::duration(
			static_cast<FileMetaData::FileTime::duration::rep>(ticks)));
	}
	Debug::Warning("Timestamp is outside file_clock's representable range, saturated",
	               LogTag);
	return ticks < minimum ? FileMetaData::FileTime::min() : FileMetaData::FileTime::max();
}

} // namespace

std::wstring WindowsMetaDataUtils::ToWide(const std::filesystem::path& path) {
	return path.wstring();
}

FileMetaData::FileTime WindowsMetaDataUtils::ToFileTime(const FILETIME& value) {
	ULARGE_INTEGER stamped;
	stamped.LowPart = value.dwLowDateTime;
	stamped.HighPart = value.dwHighDateTime;

	// 先按 64 位上限截断，避免超出 int64 的输入在相减时回绕
	const std::uint64_t capped = stamped.QuadPart > static_cast<std::uint64_t>(
	                                                  std::numeric_limits<std::int64_t>::max())
	                                 ? static_cast<std::uint64_t>(
	                                       std::numeric_limits<std::int64_t>::max())
	                                 : stamped.QuadPart;
	const std::int64_t unixTicks = static_cast<std::int64_t>(capped) -
	                               WindowsEpochToUnixTicks;

	const ClockBases& bases = GetClockBases();
	const auto systemTime =
		std::chrono::system_clock::time_point(HundredNanoseconds(unixTicks));
	const auto delta = std::chrono::duration_cast<FileMetaData::FileTime::duration>(
	    systemTime - bases.system);

	return SaturateToFileTime(static_cast<__int128>(bases.fileTicks) + delta.count());
}

FILETIME WindowsMetaDataUtils::ToWindowsFileTime(FileMetaData::FileTime value) {
	const ClockBases& bases = GetClockBases();
	const auto delta = value.time_since_epoch() - bases.file.time_since_epoch();
	const auto systemTime = bases.system + std::chrono::duration_cast<
	                                           std::chrono::system_clock::duration>(delta);
	const std::int64_t unixTicks =
		std::chrono::duration_cast<HundredNanoseconds>(systemTime.time_since_epoch()).count();

	// FILETIME 是无符号的 1601 起计数：早于 1601 或过大的时间同样饱和
	const __int128 ticks = static_cast<__int128>(unixTicks) + WindowsEpochToUnixTicks;
	const __int128 maximum = std::numeric_limits<std::uint64_t>::max();
	std::uint64_t clamped = 0;
	if (ticks <= 0) {
		Debug::Warning("Timestamp is before the FILETIME epoch, saturated to 0", LogTag);
	} else if (ticks > maximum) {
		Debug::Warning("Timestamp exceeds FILETIME's range, saturated", LogTag);
		clamped = std::numeric_limits<std::uint64_t>::max();
	} else {
		clamped = static_cast<std::uint64_t>(ticks);
	}

	ULARGE_INTEGER result;
	result.QuadPart = clamped;
	FILETIME fileTime;
	fileTime.dwLowDateTime = result.LowPart;
	fileTime.dwHighDateTime = result.HighPart;
	return fileTime;
}

std::string WindowsMetaDataUtils::AccountName(PSID sid) {
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

	// 账户名可能是非 ASCII（中文用户名等），必须按 UTF-8 存，
	// 不能用 path::string()（在 MSVC 上会变成 ANSI 代码页而丢字符）
	return Utf8::FromWide(name.data());
}

std::uint64_t WindowsMetaDataUtils::AccountId(PSID sid) {
	if (sid == nullptr || !IsValidSid(sid)) {
		return 0;
	}
	const auto count = *GetSidSubAuthorityCount(sid);
	return count == 0 ? 0 : *GetSidSubAuthority(sid, count - 1);
}

FileType WindowsMetaDataUtils::DetectType(const std::filesystem::path& path,
                                          DWORD attributes, HANDLE handle) {
	if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
		return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? FileType::Directory
		                                                    : FileType::Regular;
	}

	FILE_ATTRIBUTE_TAG_INFO tagInfo{};
	if (GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tagInfo,
	                                 sizeof(tagInfo)) == FALSE) {
		// 查不到标记就无从判断，只能落到 Other，原因值得留痕
		Debug::Warning("Failed to query reparse tag: " + path.string() + " (win32 error " +
		                   std::to_string(GetLastError()) + ")",
		               LogTag);
		return FileType::Other;
	}
	switch (tagInfo.ReparseTag) {
	case IO_REPARSE_TAG_SYMLINK:
		return FileType::Symlink;
	case IO_REPARSE_TAG_MOUNT_POINT:
		// 目录联接（junction）不需要特权即可重建，与符号链接区分开
		return FileType::Junction;
	case IO_REPARSE_TAG_AF_UNIX:
		// Windows 10 1803+ 的 AF_UNIX 套接字会落成带此标记的文件（无载荷）
		return FileType::Socket;
	default:
		// 其余重解析点（云占位文件、AppExecLink 等）无法重建
		return FileType::Other;
	}
}

bool WindowsMetaDataUtils::ReadSecurity(const std::wstring& path, FileMetaData& metadata) {
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
