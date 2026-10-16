#include "NameFilter.h"

#include "../../../General/Debug.h"
#include "../../../General/Utf8.h"

#include <cstddef>
#include <utility>

NameFilter::NameFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool NameFilter::ParseParameters(const std::string parameters) {
	shape = TextMatch::Shape::Exact;
	head.clear();
	tail.clear();

	// 模式 -> 形状 + 字面串。字面串顺手转成原生字符并小写化，
	// Match 里就既没有转码也没有分配了
	std::string headUtf8;
	std::string tailUtf8;
	if (!TextMatch::Split(parameters, shape, headUtf8, tailUtf8)) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": bad parameters '" + parameters +
		                 "' (expected 名字 / *.ext / 名字* / *名字*)",
		             LogTag);
		return false;
	}
	head = TextMatch::NativeLiteral(headUtf8);
	tail = TextMatch::NativeLiteral(tailUtf8);

	SetValid(true);
	return true;
}

bool NameFilter::Match(const FileMetaData& meta) const {
	return TextMatch::Matches<NativeChar>(shape, LastComponent(meta.RelativePath()), head, tail);
}

NameFilter::NativeView NameFilter::LastComponent(const std::filesystem::path& path) noexcept {
	const NativeString& full = path.native();
	std::size_t begin = full.size();
	while (begin > 0 && !TextMatch::IsSeparator(full[begin - 1])) {
		--begin;
	}
	return NativeView(full.data() + begin, full.size() - begin);
}

std::string NameFilter::ToString() const {
	// 参数文本由"形状 + 字面串"还原（基类不再存原文）。字面串存的是 path 的原生字符
	// （Windows 上是 UTF-16），拼串前转回 UTF-8；小写化是解析时做的，所以还原出来是小写形式
	return Name() + ":" + TextMatch::Compose(shape, Utf8::FromPath(std::filesystem::path(head)),
	                                         Utf8::FromPath(std::filesystem::path(tail)));
}
