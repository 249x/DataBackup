#include "SizeFilter.h"

#include "TextMatch.h"

#include "../../../General/Debug.h"

#include <cctype>
#include <limits>
#include <cstddef>
#include <utility>

SizeFilter::SizeFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool SizeFilter::ParseSize(const std::string& text, std::uintmax_t& out) {
	if (text.empty()) {
		return false;
	}

	const std::uintmax_t limit = std::numeric_limits<std::uintmax_t>::max();
	std::uintmax_t value = 0;
	std::size_t at = 0;
	bool anyDigit = false;

	for (; at < text.size(); ++at) {
		const char c = text[at];
		if (c < '0' || c > '9') {
			break;
		}
		const std::uintmax_t digit = static_cast<std::uintmax_t>(c - '0');
		if (value > (limit - digit) / 10) {
			return false; // 数字本身就溢出了
		}
		value = value * 10 + digit;
		anyDigit = true;
	}
	if (!anyDigit) {
		return false;
	}

	std::uintmax_t factor = 1;
	if (at < text.size()) {
		switch (TextMatch::LowerAscii(text[at])) {
		case 'k':
			factor = 1024ull;
			break;
		case 'm':
			factor = 1024ull * 1024ull;
			break;
		case 'g':
			factor = 1024ull * 1024ull * 1024ull;
			break;
		default:
			return false; // 认不出的单位
		}
		++at;
		if (at < text.size() && TextMatch::LowerAscii(text[at]) == 'b') {
			++at; // 允许 "1kb" / "2Mb"
		}
	}
	if (at != text.size()) {
		return false;
	}

	if (value > limit / factor) {
		return false; // 换算成字节后溢出
	}
	out = value * factor;
	return true;
}

bool SizeFilter::ParseCondition(const std::string& token, Kind& kind, std::uintmax_t& low,
                                std::uintmax_t& high) {
	const std::size_t rangeAt = token.find("..");
	if (rangeAt != std::string::npos) {
		if (!ParseSize(token.substr(0, rangeAt), low) ||
		    !ParseSize(token.substr(rangeAt + 2), high)) {
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
		return false; // 没有 min=/max=/= 又没有区间
	}

	std::uintmax_t value = 0;
	if (!ParseSize(token.substr(equalsAt + 1), value)) {
		return false;
	}

	const std::string key = token.substr(0, equalsAt);
	if (key.empty()) {
		kind = Kind::Exact;
	} else if (key == "min") {
		kind = Kind::Min;
	} else if (key == "max") {
		kind = Kind::Max;
	} else {
		return false;
	}
	low = value;
	high = value;
	return true;
}

bool SizeFilter::ParseParameters(const std::string parameters) {
	hasMin = false;
	hasMax = false;
	lower = 0;
	upper = 0;

	std::size_t at = 0;
	const auto isSplit = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };

	while (at < parameters.size()) {
		while (at < parameters.size() && isSplit(parameters[at])) {
			++at;
		}
		if (at >= parameters.size()) {
			break;
		}
		// 一个条件记号不允许带空格（"min=1M max=2M" 是两个记号），所以直接切到空白
		std::size_t end = at;
		while (end < parameters.size() && !isSplit(parameters[end])) {
			++end;
		}

		const std::string token = parameters.substr(at, end - at);
		Kind kind = Kind::Exact;
		std::uintmax_t low = 0;
		std::uintmax_t high = 0;
		const bool parsed = ParseCondition(token, kind, low, high);

		bool accepted = parsed;
		switch (kind) {
		case Kind::Range:
		case Kind::Exact:
			// 这两个自带上下界，只能单独出现
			accepted = parsed && !hasMin && !hasMax;
			if (accepted) {
				lower = low;
				upper = high;
				hasMin = true;
				hasMax = true;
			}
			break;
		case Kind::Min:
			accepted = parsed && !hasMin;
			if (accepted) {
				lower = low;
				hasMin = true;
			}
			break;
		case Kind::Max:
			accepted = parsed && !hasMax;
			if (accepted) {
				upper = high;
				hasMax = true;
			}
			break;
		}

		if (!accepted) {
			hasMin = false;
			hasMax = false;
			SetValid(false);
			Debug::Error("Filter #" + std::to_string(Id()) + ": bad parameters '" + parameters + "' (bad '" +
			                 token + "'; expected min=1M / max=4K / =0 / 1M..2M, units k/m/g)",
			             LogTag);
			return false;
		}

		at = end;
	}

	if (!hasMin && !hasMax) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": bad parameters '" + parameters +
		                 "' (expected min=1M / max=4K / =0 / 1M..2M, units k/m/g)",
		             LogTag);
		return false;
	}

	SetValid(true);
	return true;
}

bool SizeFilter::Match(const FileMetaData& meta) const {
	const std::uintmax_t size = meta.Size();
	if (hasMin && size < lower) {
		return false;
	}
	if (hasMax && upper < size) {
		return false;
	}
	return true;
}

std::string SizeFilter::ToString() const {
	return Name() + ":" + RawParameters();
}
