#pragma once

#include "Filter.h"
#include "TextMatch.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

//
// 按**条目相对路径**过滤（含目录部分）。
//
// 模式里只有 '*' 和 '?' 是元字符，其余字符（含 '_'）都是普通字符：
//
//   path:src/report.txt      全等
//   path:src/*               前缀 "src/"（通配只在末尾）
//   path:*.tmp               后缀 ".tmp"
//   path:*/temp/*            子串 "/temp/"（两端通配）
//   path:src/*.csv           前缀 "src/" + 后缀 ".csv"（中间一个 '*'）
//   path:a?c*                含 '?' 或多个 '*'：通用逐字符通配
//
// 模式里统一写 '/'：路径里的 '/' 与 '\' 都算与它相同（Windows 两种分隔符都会出现）。
// '*' 会跨过分隔符，所以 "path:*.tmp" 就是"任意层级的 .tmp"。比较忽略 ASCII 大小写。
//
// 模式解释成"形状 + 字面串"由 TextMatch 做，这里只在 ParseParameters 里转一次码；
// Match 直接对 RelativePath().native() 取视图来比，不构造 filename()、不转 UTF-8、不分配。
//
//
class PathFilter : public Filter {
public:
	PathFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	// path 的原生字符类型（Windows 上是 UTF-16；宽窄由标准库决定，这里不写死）
	using NativeString = std::filesystem::path::string_type;
	using NativeChar = NativeString::value_type;

	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;

	TextMatch::Shape shape = TextMatch::Shape::Exact;
	// 已小写化的字面串；Affix 时 head 是前缀、tail 是后缀
	NativeString head;
	NativeString tail;
};
