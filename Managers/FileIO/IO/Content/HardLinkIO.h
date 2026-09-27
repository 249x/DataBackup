#pragma once

#include "../../../../FileStruct/FileEntry.h"

#include <filesystem>
#include <optional>
#include <vector>

class FileContentIO;

/*
 * 硬链接的处理。硬链接不是重解析点（与符号链接/junction 无关），故单独成类。
 *
 * "同一份文件"的判定依据是来源卷上的 (FileMetaData::DeviceId, FileMetaData::FileId)，
 * 同组条目该值必然相同——因此不需要额外记录"对端路径"，格式无需改动。
 */
class HardLinkIO {
public:
	HardLinkIO() = default;

	// 打包端：同一份文件的多个路径只保留一份内容，组内其余成员的内容被清空并释放
	void Deduplicate(std::vector<FileEntry>& entries) const;

	// 还原端：把同组条目改建成硬链接
	void Relink(const std::vector<FileEntry>& entries,
	            const std::vector<std::optional<std::filesystem::path>>& targets,
	            const FileContentIO& contentIO) const;

	// 把 linkPath 处已写好的普通文件改造成指向 existingPath 的硬链接。
	// 失败时原副本会被保留（内容仍正确，只是多占空间），由调用方决定是否告警
	bool Replace(const std::filesystem::path& linkPath,
	             const std::filesystem::path& existingPath) const;
};
