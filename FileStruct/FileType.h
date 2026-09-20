#pragma once

#include <cstdint>

/*
 * 文件类型：标识普通文件与特殊文件。
 *
 * 排列顺序即内容语义分组：文件字节 -> 链接目标 -> 无载荷。
 * FileEntry::Content() 的语义随类型变化：
 *   Regular / HardLink            -> 文件字节内容
 *   Symlink / Junction            -> 链接目标路径（UTF-8 编码字节）
 *   Directory / 设备 / 管道 / 套接字 / Other -> 空（仅作标记）
 *
 * 该枚举是项目自有类型且固定底层宽度，序列化值不依赖
 * std::filesystem::file_type（其取值由实现定义）。
 * 取值会随重构调整，因此改动时必须同步提升
 * FileMetaData::SerializationVersion，避免旧归档被错读。
 */
enum class FileType : std::uint8_t {
	None = 0,             // 未设置
	Regular = 1,          // 普通文件
	Directory = 2,        // 目录
	Symlink = 3,          // 符号链接
	Junction = 4,         // NTFS 目录联接（挂载点），同为目录链接
	HardLink = 5,         // 硬链接，内容与普通文件相同
	Fifo = 6,             // 命名管道
	Socket = 7,           // 套接字
	BlockDevice = 8,      // 块设备
	CharacterDevice = 9,  // 字符设备
	Other = 10,           // 未识别 / 无法重建，仅作标记
};

// 内容为文件字节的类型
constexpr bool IsFileContentType(FileType type) {
	return type == FileType::Regular || type == FileType::HardLink;
}

// 内容为链接目标路径的类型
constexpr bool IsLinkType(FileType type) {
	return type == FileType::Symlink || type == FileType::Junction;
}

constexpr bool IsValidFileTypeValue(std::uint8_t value) {
	switch (static_cast<FileType>(value)) {
	case FileType::None:
	case FileType::Regular:
	case FileType::Directory:
	case FileType::Symlink:
	case FileType::Other:
	case FileType::Junction:
	case FileType::HardLink:
	case FileType::BlockDevice:
	case FileType::CharacterDevice:
	case FileType::Fifo:
	case FileType::Socket:
		return true;
	}
	return false;
}

constexpr const char* FileTypeName(FileType type) {
	switch (type) {
	case FileType::None:
		return "none";
	case FileType::Regular:
		return "regular";
	case FileType::Directory:
		return "directory";
	case FileType::Symlink:
		return "symlink";
	case FileType::Other:
		return "other";
	case FileType::Junction:
		return "junction";
	case FileType::HardLink:
		return "hardlink";
	case FileType::BlockDevice:
		return "block_device";
	case FileType::CharacterDevice:
		return "character_device";
	case FileType::Fifo:
		return "fifo";
	case FileType::Socket:
		return "socket";
	}
	return "invalid";
}
