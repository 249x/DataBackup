#pragma once

#include "../../../../FileStruct/FileMetaData.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

/*
 * Windows 元数据细节：路径/时间换算、SID 名称、重解析标记判定、安全信息读取。
 */
class WindowsMetaDataUtils {
public:
	WindowsMetaDataUtils() = default;

	static constexpr const char* LogTag = "MetaData";

	std::wstring ToWide(const std::filesystem::path& path) const;

	// FILETIME（1601-01-01 起的百纳秒）与 file_clock 互转。
	FileMetaData::FileTime ToFileTime(const FILETIME& value) const;
	FILETIME ToWindowsFileTime(FileMetaData::FileTime value) const;

	std::string AccountName(PSID sid) const;
	std::uint64_t AccountId(PSID sid) const;

	FileType DetectType(const std::filesystem::path& path, DWORD attributes,
	                    HANDLE handle) const;

	bool ReadSecurity(const std::wstring& path, FileMetaData& metadata) const;

private:
	// SID 字节内容 -> 账户名。SID 到名字要查 LSA/SAM，实测占单文件元数据开销的
	// 2/3（4 次/文件的 LookupAccountSidW）；而一棵树里不同 SID 通常只有一两个，
	// 缓存后上万次查询塌缩成个位数。AccountName 是 const，所以这里必须 mutable
	mutable std::unordered_map<std::string, std::string> accountNames;
};
