#include "TypeFilter.h"

#include "TextMatch.h"

#include "../../../General/Debug.h"

#include <cctype>
#include <utility>

TypeFilter::TypeFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool TypeFilter::ParseNames(const std::string& text, std::uint16_t& mask) {
	std::uint16_t result = 0;
	std::size_t at = 0;

	while (at < text.size()) {
		// 逗号与空白都当分隔符，免得 "regular,hardlink" 白报一次错
		const auto isSplit = [](char c) {
			return c == ',' || std::isspace(static_cast<unsigned char>(c)) != 0;
		};
		while (at < text.size() && isSplit(text[at])) {
			++at;
		}
		if (at >= text.size()) {
			break;
		}

		std::size_t end = at;
		while (end < text.size() && !isSplit(text[end])) {
			++end;
		}

		std::string name = text.substr(at, end - at);
		for (char& c : name) {
			c = TextMatch::LowerAscii(c);
		}

		FileType type = FileType::None;
		if (!FileTypeFromName(name, type)) {
			return false;
		}
		const unsigned bit = static_cast<unsigned>(type);
		if (bit >= BitCount) {
			return false;
		}
		result = static_cast<std::uint16_t>(result | (1u << bit));

		at = end;
	}

	if (result == 0) {
		return false; // 一个名字都没给
	}
	mask = result;
	return true;
}

bool TypeFilter::ParseParameters(const std::string parameters) {
	mask = 0;

	if (!ParseNames(parameters, mask)) {
		mask = 0;
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": bad parameters '" + parameters +
		                 "' (expected 类型名，如 regular / directory / symlink,junction)",
		             LogTag);
		return false;
	}

	SetValid(true);
	return true;
}

bool TypeFilter::Match(const FileMetaData& meta) const {
	const unsigned value = static_cast<unsigned>(meta.Type());
	if (value >= BitCount) {
		return false; // 取值异常：不认，也不做移位
	}
	return (mask & (1u << value)) != 0;
}

std::string TypeFilter::ToString() const {
	return Name() + ":" + RawParameters();
}
