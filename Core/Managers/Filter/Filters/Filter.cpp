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

const std::string& Filter::RawParameters() const noexcept {
	return rawParameters;
}


bool Filter::Check(const FileMetaData& meta) const{
	if(!valid) return true;
	return Match(meta);
}

bool Filter::SetParameters(const std::string parameters) {
	rawParameters = parameters;
	return ParseParameters(std::move(parameters));
}

bool Filter::IsValid() const noexcept {
	return valid;
}

void Filter::SetValid(bool valid) noexcept{
	this->valid = valid;
}

