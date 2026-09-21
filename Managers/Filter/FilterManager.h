#pragma once

#include "../Manager.h"
#include "FilterRule.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class FileMetaData;

class FilterManager : public Manager {
public:
	// 统一的过滤条件，所有字段可选
	struct Options {
		std::optional<std::string> path;
		FilterRule::MatchMode pathMode = FilterRule::MatchMode::Glob;

		std::optional<std::string> name;
		FilterRule::MatchMode nameMode = FilterRule::MatchMode::Glob;

		std::optional<FileType> type;

		std::optional<FileMetaData::FileTime> timeFrom;
		std::optional<FileMetaData::FileTime> timeTo;

		std::optional<std::uintmax_t> sizeMin;
		std::optional<std::uintmax_t> sizeMax;

		std::optional<std::string> owner;
		FilterRule::MatchMode ownerMode = FilterRule::MatchMode::Exact;
	};

	FilterManager(System& sys);
	~FilterManager() override;

	void Initialize() override;

	// ---- 唯一入口 ----
	void AddRule(const Options& options);
	void AddRule(const FilterRule& rule);
	void ClearRules();
	bool HasRules() const noexcept;
	std::size_t RuleCount() const noexcept;

	// ---- 过滤 ----
	std::vector<FileMetaData> Filter(const std::vector<FileMetaData>& entries) const;
	bool ShouldInclude(const FileMetaData& meta) const;

private:
	std::vector<FilterRule> rules;
};