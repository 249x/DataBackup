#include "FilterManager.h"

#include "../../FileStruct/FileMetaData.h"

FilterManager::FilterManager(System& sys) : Manager(sys) {}

FilterManager::~FilterManager() = default;

void FilterManager::Initialize() {
}

void FilterManager::AddRule(const Options& options) {
	FilterRule rule;

	if (options.path.has_value()) {
		rule.SetPathPattern(*options.path, options.pathMode);
	}
	if (options.name.has_value()) {
		rule.SetNamePattern(*options.name, options.nameMode);
	}
	if (options.type.has_value()) {
		rule.SetType(*options.type);
	}
	if (options.timeFrom.has_value() && options.timeTo.has_value()) {
		rule.SetTimeRange(*options.timeFrom, *options.timeTo);
	}
	if (options.sizeMin.has_value() && options.sizeMax.has_value()) {
		rule.SetSizeRange(*options.sizeMin, *options.sizeMax);
	}
	if (options.owner.has_value()) {
		rule.SetOwner(*options.owner, options.ownerMode);
	}

	AddRule(rule);
}

void FilterManager::AddRule(const FilterRule& rule) {
	rules.push_back(rule);
}

void FilterManager::ClearRules() {
	rules.clear();
}

bool FilterManager::HasRules() const noexcept {
	return !rules.empty();
}

std::size_t FilterManager::RuleCount() const noexcept {
	return rules.size();
}

bool FilterManager::ShouldInclude(const FileMetaData& meta) const {
	if (rules.empty()) {
		return true;
	}
	for (const FilterRule& rule : rules) {
		if (rule.Match(meta)) {
			return true;
		}
	}
	return false;
}

std::vector<FileMetaData> FilterManager::Filter(const std::vector<FileMetaData>& entries) const {
	if (rules.empty()) {
		return entries;
	}
	std::vector<FileMetaData> result;
	result.reserve(entries.size());
	for (const FileMetaData& meta : entries) {
		if (ShouldInclude(meta)) {
			result.push_back(meta);
		}
	}
	return result;
}