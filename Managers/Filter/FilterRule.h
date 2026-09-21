#pragma once

#include "../../FileStruct/FileMetaData.h"
#include "../../FileStruct/FileType.h"

#include <cstdint>
#include <filesystem>
#include <string>

class FilterRule {
public:
	enum class MatchMode {
		Glob,
		Prefix,
		Exact,
		Contains
	};

	FilterRule() = default;
	~FilterRule() = default;

	void SetPathPattern(const std::string& pattern, MatchMode mode = MatchMode::Glob);
	bool HasPathPattern() const noexcept;

	void SetNamePattern(const std::string& pattern, MatchMode mode = MatchMode::Glob);
	bool HasNamePattern() const noexcept;

	void SetType(FileType type);
	bool HasType() const noexcept;

	void SetTimeRange(FileMetaData::FileTime from, FileMetaData::FileTime to);
	bool HasTimeRange() const noexcept;

	void SetSizeRange(std::uintmax_t min, std::uintmax_t max);
	bool HasSizeRange() const noexcept;

	void SetOwner(const std::string& owner, MatchMode mode = MatchMode::Exact);
	bool HasOwner() const noexcept;

	// 改：接收 FileMetaData
	bool Match(const FileMetaData& meta) const;

private:
	static bool MatchString(const std::string& value, const std::string& pattern,
	                        MatchMode mode);

	std::string pathPattern;
	MatchMode pathMode = MatchMode::Glob;
	bool hasPath = false;

	std::string namePattern;
	MatchMode nameMode = MatchMode::Glob;
	bool hasName = false;

	FileType type = FileType::None;
	bool hasType = false;

	FileMetaData::FileTime timeFrom{};
	FileMetaData::FileTime timeTo{};
	bool hasTime = false;

	std::uintmax_t sizeMin = 0;
	std::uintmax_t sizeMax = 0;
	bool hasSize = false;

	std::string ownerPattern;
	MatchMode ownerMode = MatchMode::Exact;
	bool hasOwner = false;
};