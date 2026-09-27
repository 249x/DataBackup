#include "PathFilter.h"

#include "../../../General/Debug.h"

#include <filesystem>
#include <utility>

PathFilter::PathFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool PathFilter::ParseParameters(const std::string parameters) {
	shape = TextMatch::Shape::Exact;
	head.clear();
	tail.clear();

	std::string headUtf8;
	std::string tailUtf8;
	if (!TextMatch::Split(parameters, shape, headUtf8, tailUtf8)) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": bad parameters '" + parameters +
		                 "' (expected 路径 / *.ext / src/* / *子串*)",
		             LogTag);
		return false;
	}
	head = TextMatch::NativeLiteral(headUtf8);
	tail = TextMatch::NativeLiteral(tailUtf8);

	SetValid(true);
	return true;
}

bool PathFilter::Match(const FileMetaData& meta) const {
	TextMatch::Options options;
	options.separatorsEqual = true; // 模式写 '/'，路径里可能是 '\'
	return TextMatch::Matches<NativeChar>(shape, meta.RelativePath().native(), head, tail, options);
}

std::string PathFilter::ToString() const {
	return Name() + ":" + RawParameters();
}
