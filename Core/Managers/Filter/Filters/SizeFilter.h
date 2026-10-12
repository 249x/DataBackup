#pragma once

#include "Filter.h"

#include <cstdint>
#include <string>

/*
 * 按**字节大小**过滤（FileMetaData::Size()）。
 *
 *   size:min=1M               >= 1 MiB
 *   size:max=4K               <= 4 KiB
 *   size:min=1M max=2M        闭区间
 *   size:1M..2M               闭区间（区间写法）
 *   size:=0                   恰好 0 字节（空文件）
 *
 * 单位后缀可选、按二进制（1k = 1024）：k / m / g，后面可以再跟一个 b（"1kb" 同 "1k"）；
 * 不带后缀就是字节数。"1M..2M" 写反了也照样能用。
 *
 * 比较符只有 min= / max= / = 三种，'=' 不能和 min=/max= 混用（避开 '<' '>' 是因为
 * 它们在 cmd/PowerShell 里会被当成重定向）。解析失败按"参数坏掉"处理——基类的
 * Check 对无效条目是放行。
 *
 * ParseParameters 里就把条件折成上下界两个数，Match 只需两次比较。
 */
class SizeFilter : public Filter {
public:
	SizeFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;

	// 一个条件记号解析出来的形状
	enum class Kind {
		Range,  // "A..B"
		Min,    // "min=V"
		Max,    // "max=V"
		Exact,  // "=V"
	};

	static char LowerAscii(char c) noexcept;
	// "1024" / "1k" / "2Mb" -> 字节数；格式不对或溢出返回 false
	static bool ParseSize(const std::string& text, std::uintmax_t& out);
	// 记号的形状与端点值；值本身由 ParseSize 解释
	static bool ParseCondition(const std::string& token, Kind& kind, std::uintmax_t& low,
	                           std::uintmax_t& high);
	bool hasMin = false;
	bool hasMax = false;
	std::uintmax_t lower = 0;
	std::uintmax_t upper = 0;
};
