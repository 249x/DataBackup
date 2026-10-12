#include "OwnerFilter.h"

#include "../../../General/Debug.h"

#include <utility>

OwnerFilter::OwnerFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool OwnerFilter::ParseParameters(const std::string parameters) {
	shape = TextMatch::Shape::Exact;
	head.clear();
	tail.clear();

	std::string headUtf8;
	std::string tailUtf8;
	if (!TextMatch::Split(parameters, shape, headUtf8, tailUtf8)) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": bad parameters '" + parameters +
		                 "' (expected 账户名 / *子串* / 前缀* / *.后缀)",
		             LogTag);
		return false;
	}
	head = TextMatch::LowerLiteral(headUtf8);
	tail = TextMatch::LowerLiteral(tailUtf8);

	SetValid(true);
	return true;
}

bool OwnerFilter::Match(const FileMetaData& meta) const {
	// 属主本身就是 UTF-8：直接拿来比，没有转码也没有分配
	return TextMatch::Matches<char>(shape, meta.Owner(), head, tail);
}

std::string OwnerFilter::ToString() const {
	return Name() + ":" + RawParameters();
}
