#include "TimeFilter.h"

#include "TextMatch.h"

#include "../../../General/Debug.h"

#include <cctype>
#include <chrono>
#include <cstddef>
#include <utility>

TimeFilter::TimeFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool TimeFilter::DigitsAt(const std::string& text, std::size_t at, std::size_t count, int& out) {
	if (at + count > text.size()) {
		return false;
	}
	int value = 0;
	for (std::size_t i = 0; i < count; ++i) {
		const char c = text[at + i];
		if (c < '0' || c > '9') {
			return false;
		}
		value = value * 10 + (c - '0');
	}
	out = value;
	return true;
}

bool TimeFilter::ParseDate(const std::string& text, std::tm& out) {
	std::tm tm{};
	if (!DigitsAt(text, 0, 4, tm.tm_year) || text.size() < 5 || text[4] != '-' ||
	    !DigitsAt(text, 5, 2, tm.tm_mon) || text.size() < 8 || text[7] != '-' ||
	    !DigitsAt(text, 8, 2, tm.tm_mday)) {
		return false;
	}
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	if (text.size() > 10) {
		if (text[10] != 'T' || !DigitsAt(text, 11, 2, tm.tm_hour) || text.size() < 14 ||
		    text[13] != ':' || !DigitsAt(text, 14, 2, tm.tm_min)) {
			return false;
		}
		if (text.size() > 16) {
			if (text[16] != ':' || !DigitsAt(text, 17, 2, tm.tm_sec) || text.size() != 19) {
				return false;
			}
		} else if (text.size() != 16) {
			return false;
		}
	} else if (text.size() != 10) {
		return false;
	}

	if (tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1 || tm.tm_mday > 31 || tm.tm_hour < 0 ||
	    tm.tm_hour > 23 || tm.tm_min < 0 || tm.tm_min > 59 || tm.tm_sec < 0 || tm.tm_sec > 60) {
		return false;
	}

	tm.tm_isdst = -1; // 让 mktime 自己判断夏令时，不要沿用上一个 tm 的取值
	out = tm;
	return true;
}

TimeFilter::FileTime TimeFilter::FromCalendar(const std::tm& tm) {
	/*
	 * time_t -> FileTime。
	 *
	 * C++17 没有两个时钟之间的标准换算（std::chrono::clock_cast 是 C++20），而 Windows 上
	 * file_clock 的起点与 system_clock 不同（本机是 Unix 纪元 + 6437664000s）。这里在首次
	 * 使用时采样一对基准点，之后按线性关系平移——两个 now() 之间只隔几十纳秒，对"天"这一
	 * 粒度的比较毫无影响。
	 */
	struct Anchor {
		std::chrono::system_clock::time_point system;
		FileTime file;
	};
	static const Anchor anchor = [] {
		return Anchor{std::chrono::system_clock::now(), FileTime::clock::now()};
	}();

	std::tm local = tm;
	const std::time_t seconds = std::mktime(&local);
	if (seconds == static_cast<std::time_t>(-1)) {
		return FileTime::min();
	}

	const auto system = std::chrono::system_clock::from_time_t(seconds);
	return anchor.file + std::chrono::duration_cast<FileTime::duration>(system - anchor.system);
}

bool TimeFilter::ParsePoint(const std::string& text, FileTime& out) {
	std::tm tm{};
	if (!ParseDate(text, tm)) {
		return false;
	}
	out = FromCalendar(tm);
	return true;
}

bool TimeFilter::ParseField(const std::string& text, Field& field) noexcept {
	if (EqualsIgnoreCase(text, "write")) {
		field = Field::Write;
	} else if (EqualsIgnoreCase(text, "access")) {
		field = Field::Access;
	} else if (EqualsIgnoreCase(text, "create")) {
		field = Field::Create;
	} else {
		return false;
	}
	return true;
}

bool TimeFilter::EqualsIgnoreCase(const std::string& text, const char* lower) noexcept {
	std::size_t i = 0;
	for (; i < text.size() && lower[i] != '\0'; ++i) {
		if (TextMatch::LowerAscii(text[i]) != lower[i]) {
			return false;
		}
	}
	return i == text.size() && lower[i] == '\0';
}

bool TimeFilter::ParseCondition(const std::string& token, Kind& kind, FileTime& low,
                                FileTime& high) {
	const std::size_t rangeAt = token.find("..");
	if (rangeAt != std::string::npos) {
		if (!ParsePoint(token.substr(0, rangeAt), low) || !ParsePoint(token.substr(rangeAt + 2), high)) {
			return false;
		}
		if (high < low) {
			std::swap(low, high); // 写反了也照样用
		}
		kind = Kind::Range;
		return true;
	}

	const std::size_t equalsAt = token.find('=');
	if (equalsAt == std::string::npos) {
		return false;
	}

	FileTime value{};
	if (!ParsePoint(token.substr(equalsAt + 1), value)) {
		return false;
	}

	const std::string key = token.substr(0, equalsAt);
	if (key == "min") {
		kind = Kind::Min;
	} else if (key == "max") {
		kind = Kind::Max;
	} else {
		return false; // 空键（"="）也不收：时间点相等没有意义
	}
	low = value;
	high = value;
	return true;
}

bool TimeFilter::ParseParameters(const std::string parameters) {
	field = Field::Write;
	hasMin = false;
	hasMax = false;

	const auto isSplit = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };

	std::size_t at = 0;
	bool first = true;
	bool any = false;

	while (at < parameters.size()) {
		while (at < parameters.size() && isSplit(parameters[at])) {
			++at;
		}
		if (at >= parameters.size()) {
			break;
		}

		std::size_t end = at;
		while (end < parameters.size() && !isSplit(parameters[end])) {
			++end;
		}
		const std::string token = parameters.substr(at, end - at);
		const std::size_t next = end;

		// 首个记号可能是字段名，但后面必须还有条件——否则 "time:write" 只是想比一个叫
		// write 的日期，应该交给下面照常报错
		bool isField = false;
		if (first) {
			first = false;
			std::size_t probe = next;
			while (probe < parameters.size() && isSplit(parameters[probe])) {
				++probe;
			}
			Field parsed = Field::Write;
			if (probe < parameters.size() && ParseField(token, parsed)) {
				field = parsed;
				isField = true;
			}
		}
		at = next;
		if (isField) {
			continue;
		}

		Kind kind = Kind::Min;
		FileTime low{};
		FileTime high{};
		bool accepted = ParseCondition(token, kind, low, high);
		if (accepted) {
			if (kind == Kind::Range) {
				accepted = !hasMin && !hasMax; // 区间自带两端，只能单独出现
				if (accepted) {
					lower = low;
					upper = high;
					hasMin = true;
					hasMax = true;
				}
			} else if (kind == Kind::Min) {
				accepted = !hasMin;
				if (accepted) {
					lower = low;
					hasMin = true;
				}
			} else {
				accepted = !hasMax;
				if (accepted) {
					upper = high;
					hasMax = true;
				}
			}
		}

		if (!accepted) {
			hasMin = false;
			hasMax = false;
			SetValid(false);
			Debug::Error("Filter #" + std::to_string(Id()) + ": bad parameters '" + parameters + "' (bad '" +
			                 token +
			                 "'; expected min=2024-01-01 / max=2024-12-31 / "
			                 "2024-01-01..2024-12-31, optional prefix write|access|create)",
			             LogTag);
			return false;
		}
		any = true;
	}

	if (!any) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": bad parameters '" + parameters +
		                 "' (expected min=2024-01-01 / max=2024-12-31 / 2024-01-01..2024-12-31, "
		                 "optional prefix write|access|create)",
		             LogTag);
		return false;
	}

	SetValid(true);
	return true;
}

TimeFilter::FileTime TimeFilter::TimeOf(const FileMetaData& meta) const noexcept {
	switch (field) {
	case Field::Access:
		return meta.LastAccessTime();
	case Field::Create:
		return meta.CreationTime();
	case Field::Write:
		break;
	}
	return meta.LastWriteTime();
}

bool TimeFilter::Match(const FileMetaData& meta) const {
	const FileTime when = TimeOf(meta);
	if (hasMin && when < lower) {
		return false;
	}
	if (hasMax && upper < when) {
		return false;
	}
	return true;
}

std::string TimeFilter::ToString() const {
	return Name() + ":" + RawParameters();
}
