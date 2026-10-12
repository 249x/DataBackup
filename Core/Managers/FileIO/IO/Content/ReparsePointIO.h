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
	ReparsePointIO() = default;

	bool ReadTarget(const std::filesystem::path& path,
	                std::vector<std::uint8_t>& content) const;

	bool Create(const std::filesystem::path& link,
	            const std::filesystem::path& target) const;

	bool CreateSocket(const std::filesystem::path& path) const;
};
