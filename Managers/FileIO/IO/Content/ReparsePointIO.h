#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

/*
 * 重解析点（符号链接 / junction / AF_UNIX 套接字）的读取与创建。
 * 本工具链的 libstdc++ 未实现 std::filesystem 的符号链接接口，因此改走 Win32 API
 */
class ReparsePointIO {
public:
	ReparsePointIO() = delete;

	static bool ReadTarget(const std::filesystem::path& path,
	                       std::vector<std::uint8_t>& content);

	static bool Create(const std::filesystem::path& link,
	                   const std::filesystem::path& target);

	// 重建 AF_UNIX 套接字文件（Windows 10 1803+）。套接字是运行时对象，
	// 这里只还原文件系统侧的标记，不会有监听者
	static bool CreateSocket(const std::filesystem::path& path);
};
