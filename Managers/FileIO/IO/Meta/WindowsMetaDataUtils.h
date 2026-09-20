#pragma once

#include "../../../../FileStruct/FileMetaData.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

/*
 * Windows 元数据细节：路径/时间换算、SID 名称、重解析标记判定、安全信息读取。
 */
class WindowsMetaDataUtils {
public:
	static constexpr const char* LogTag = "MetaData";

	static std::wstring ToWide(const std::filesystem::path& path);

	// FILETIME（1601-01-01 起的百纳秒）与 file_clock 互转。
	static FileMetaData::FileTime ToFileTime(const FILETIME& value);
	static FILETIME ToWindowsFileTime(FileMetaData::FileTime value);

	static std::string AccountName(PSID sid);
	static std::uint64_t AccountId(PSID sid);

	// 判定文件类型：重解析点无法仅凭属性区分链接与其他，需要读重解析标记。
	static FileType DetectType(const std::filesystem::path& path, DWORD attributes,
	                           HANDLE handle);

	static bool ReadSecurity(const std::wstring& path, FileMetaData& metadata);
};
