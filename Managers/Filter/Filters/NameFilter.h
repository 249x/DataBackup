#pragma once

#include "Filter.h"
#include "TextMatch.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

/*
 * 按**文件名**过滤（不含目录部分）。
 *
 * 模式里只有 '*' 和 '?' 是元字符（Windows 文件名不允许这两个字符，所以不会歧义），
 * 匹配方式不写关键字，由模式串自己决定：
 *
 *   name:Makefile        全等
 *   name:*.tmp           后缀 ".tmp"（通配只在末尾，退化成后缀比较）
 *   name:log_*           前缀 "log_"（通配只在开头）
 *   name:*temp*          子串 "temp"（两端通配）
 *   name:报告*.csv       前缀 "报告" + 后缀 ".csv"（中间一个 '*'）
 *   name:a?c*            含 '?' 或多个 '*'：通用逐字符通配
 *   name:*               匹配一切
 *
 * 只看最后一段名字；要连目录一起比就用 path:。比较忽略 ASCII 大小写。
 * 模式解释成"形状 + 字面串"由 TextMatch 做，这里只在 ParseParameters 里转一次码。
 */
class NameFilter : public Filter {
public:
	NameFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	// path 的原生字符类型（Windows 上是 UTF-16；宽窄由标准库决定，这里不写死）
	using NativeString = std::filesystem::path::string_type;
	using NativeView = std::basic_string_view<NativeString::value_type>;
	using NativeChar = NativeString::value_type;

	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;

	// 从 path 里切出最后一段（视图，不分配）：不用 filename() 是因为它每次都要
	// 现构造一个 path（实测约 160 ns），而匹配本身只要几十 ns
	static NativeView LastComponent(const std::filesystem::path& path) noexcept;

	TextMatch::Shape shape = TextMatch::Shape::Exact;
	// 已小写化的字面串；Affix 时 head 是前缀、tail 是后缀
	NativeString head;
	NativeString tail;
};
