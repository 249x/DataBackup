#include "Filter.h"

#include <cstddef>
#include <sstream>
#include <utility>


Filter::Filter(const FilterManager& manager, std::string name, std::uint32_t id)
	: manager(&manager), name(std::move(name)), id(id) {
}

Filter::~Filter() = default;

const std::string& Filter::Name() const noexcept {
	return name;
}

std::uint32_t Filter::Id() const noexcept {
	return id;
}

const FilterManager& Filter::Manager() const noexcept {
	return *manager;
}

bool Filter::Check(const FileMetaData& meta) const{
	if(!valid) return true;
	return Match(meta);
}

bool Filter::SetParameters(const std::string parameters) {
	// 原文不再保存：ToString() 由各子类从解析结果还原，
	// 于是"显示的规格"与"实际的判定"不可能各说各话
	return ParseParameters(std::move(parameters));
}

bool Filter::IsValid() const noexcept {
	return valid;
}

void Filter::SetValid(bool valid) noexcept{
	this->valid = valid;
}

