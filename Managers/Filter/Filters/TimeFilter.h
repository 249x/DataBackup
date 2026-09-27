#pragma once

#include "Filter.h"

#include <cstddef>
#include <ctime>
#include <string>

/*
 * 按**时间**过滤（默认看修改时间 LastWriteTime）。
 *
 *   time:min=2024-01-01                >= 2024-01-01 00:00:00（本地时间）
 *   time:max=2024-12-31                <= 2024-12-31 00:00:00
 *   time:2024-01-01..2024-12-31        闭区间
 *   time:create max=2024-06-15         看创建时间
 *   time:access 2024-06-15..2024-06-16 看访问时间
 *
 * 字段可选 write（默认）/ access / create，写在条件之前。
 * 日期是**本地时间**（与资源管理器里显示的一致），支持 YYYY-MM-DD、YYYY-MM-DDThh:mm、
 * YYYY-MM-DDThh:mm:ss 三种精度；只写到日期时按当天 00:00:00 处理，所以
 * "max=2024-06-15" **不包含** 6 月 15 日这一天，要整天就写 "min=2024-06-15 max=2024-06-16"。
 *
 * 只有 min= / max= / A..B 三种写法：时间没有 "="（mtime 精度到 100ns，"相等"没有实际意义）；
 * 不用 '<' '>' 则是因为它们在 cmd/PowerShell 里会被当成重定向。
 *
 * ParseParameters 里就把日期换算成 file_time_type，Match 只做两次时间点比较。
 */
class TimeFilter : public Filter {
public:
	TimeFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	using FileTime = FileMetaData::FileTime;

	// 看哪一个时间
	enum class Field {
		Write,
		Access,
		Create,
	};

	// 一个条件记号解析出来的形状
	enum class Kind {
		Range,  // "A..B"
		Min,    // "min=D"
		Max,    // "max=D"
	};

	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;
	FileTime TimeOf(const FileMetaData& meta) const noexcept;

	// 从 at 处取 count 个十进制数字，不是数字则返回 false
	static bool DigitsAt(const std::string& text, std::size_t at, std::size_t count, int& out);
	// "YYYY-MM-DD" / "YYYY-MM-DDThh:mm[:ss]" -> 本地时间的日历分量
	static bool ParseDate(const std::string& text, std::tm& out);
	// 本地日历时间 -> FileTime（用一次性采样的时钟基准对换算）
	static FileTime FromCalendar(const std::tm& tm);
	static bool ParsePoint(const std::string& text, FileTime& out);
	// 字段名当关键字处理，大小写不敏感（与 type: 的名字一致）
	static bool ParseField(const std::string& text, Field& field) noexcept;
	// text 是否等于小写的关键字 lower
	static bool EqualsIgnoreCase(const std::string& text, const char* lower) noexcept;
	static bool ParseCondition(const std::string& token, Kind& kind, FileTime& low, FileTime& high);

	Field field = Field::Write;
	bool hasMin = false;
	bool hasMax = false;
	FileTime lower{};
	FileTime upper{};
};
