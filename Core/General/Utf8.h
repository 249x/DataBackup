#pragma once

#include <filesystem>
#include <string>

/*
 * 路径与文本的 UTF-8 编解码入口。
 *
 * 为什么不直接用 std::codecvt_utf8_utf16 / std::wstring_convert：
 *   它们自 C++17 起被废弃、C++26 移除；本机 GCC 16 下已报 -Wdeprecated-declarations。
 *
 * 为什么不直接写 path::u8string() / std::filesystem::u8path()：
 *   C++20 把 u8string() 的返回类型改成 std::u8string（char8_t），使
 *   `std::string s = p.u8string();` 变成硬错误；u8path 则被标记废弃。
 *   这里按 __cpp_char8_t 分支，同一份代码在 C++17 / C++20 下都能编过。
 *
 * 也不使用 path::string()：它在 MSVC 上是 ANSI 代码页，非 ASCII 会被破坏；
 * 而 u8string 在 MSVC 与 libstdc++ 上都保证是 UTF-8。
 */
class Utf8 {
public:
	Utf8() = delete;

	// 宽字符 <-> UTF-8 字节
	static std::string FromWide(const std::wstring& text) {
		return FromPath(std::filesystem::path(text));
	}
	static std::wstring ToWide(const std::string& utf8) {
		return ToPath(utf8).wstring();
	}

	// 路径 <-> UTF-8 字节。
	// 用 generic 形式（分隔符统一为 '/'），与 path::generic_string() 的分隔符一致，
	// 只是编码固定为 UTF-8，不再受平台的窄编码影响。
	static std::string FromPath(const std::filesystem::path& path) {
#if defined(__cpp_char8_t)
		const std::u8string text = path.generic_u8string();
		return std::string(text.begin(), text.end());
#else
		return path.generic_u8string();
#endif
	}

	static std::filesystem::path ToPath(const std::string& utf8) {
#if defined(__cpp_char8_t)
		return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
#else
		return std::filesystem::u8path(utf8);
#endif
	}
};
