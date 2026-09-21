#include "FilterRule.h"

#include <algorithm>
#include <cctype>

namespace {

char ToLower(char c) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool GlobMatch(const std::string& value, const std::string& pattern) {
	std::size_t v = 0;
	std::size_t p = 0;
	std::size_t starP = std::string::npos;
	std::size_t starV = 0;

	while (v < value.size()) {
		if (p < pattern.size() && (pattern[p] == '?' || ToLower(pattern[p]) == ToLower(value[v]))) {
			++v;
			++p;
		} else if (p < pattern.size() && pattern[p] == '*') {
			starP = p++;
			starV = v;
		} else if (starP != std::string::npos) {
			p = starP + 1;
			v = ++starV;
		} else {
			return false;
		}
	}
	while (p < pattern.size() && pattern[p] == '*') {
		++p;
	}
	return p == pattern.size();
}

bool PrefixMatch(const std::string& value, const std::string& pattern) {
	if (pattern.size() > value.size()) return false;
	return std::equal(pattern.begin(), pattern.end(), value.begin(),
	                  [](char a, char b) { return ToLower(a) == ToLower(b); });
}

bool ExactMatch(const std::string& value, const std::string& pattern) {
	if (value.size() != pattern.size()) return false;
	return std::equal(value.begin(), value.end(), pattern.begin(),
	                  [](char a, char b) { return ToLower(a) == ToLower(b); });
}

bool ContainsMatch(const std::string& value, const std::string& pattern) {
	if (pattern.empty()) return true;
	auto it = std::search(value.begin(), value.end(),
	                      pattern.begin(), pattern.end(),
	                      [](char a, char b) { return ToLower(a) == ToLower(b); });
	return it != value.end();
}

} // namespace

void FilterRule::SetPathPattern(const std::string& pattern, MatchMode mode) {
	pathPattern = pattern;
	pathMode = mode;
	hasPath = true;
}

bool FilterRule::HasPathPattern() const noexcept {
	return hasPath;
}

void FilterRule::SetNamePattern(const std::string& pattern, MatchMode mode) {
	namePattern = pattern;
	nameMode = mode;
	hasName = true;
}

bool FilterRule::HasNamePattern() const noexcept {
	return hasName;
}

void FilterRule::SetType(FileType t) {
	type = t;
	hasType = true;
}

bool FilterRule::HasType() const noexcept {
	return hasType;
}

void FilterRule::SetTimeRange(FileMetaData::FileTime from, FileMetaData::FileTime to) {
	timeFrom = from;
	timeTo = to;
	hasTime = true;
}

bool FilterRule::HasTimeRange() const noexcept {
	return hasTime;
}

void FilterRule::SetSizeRange(std::uintmax_t min, std::uintmax_t max) {
	sizeMin = min;
	sizeMax = max;
	hasSize = true;
}

bool FilterRule::HasSizeRange() const noexcept {
	return hasSize;
}

void FilterRule::SetOwner(const std::string& owner, MatchMode mode) {
	ownerPattern = owner;
	ownerMode = mode;
	hasOwner = true;
}

bool FilterRule::HasOwner() const noexcept {
	return hasOwner;
}

bool FilterRule::MatchString(const std::string& value, const std::string& pattern,
                             MatchMode mode) {
	switch (mode) {
	case MatchMode::Glob:     return GlobMatch(value, pattern);
	case MatchMode::Prefix:   return PrefixMatch(value, pattern);
	case MatchMode::Exact:    return ExactMatch(value, pattern);
	case MatchMode::Contains: return ContainsMatch(value, pattern);
	}
	return false;
}

bool FilterRule::Match(const FileMetaData& meta) const {
	if (hasPath) {
		if (!MatchString(meta.RelativePath().generic_string(), pathPattern, pathMode)) {
			return false;
		}
	}

	if (hasName) {
		const std::string name = meta.RelativePath().filename().string();
		if (!MatchString(name, namePattern, nameMode)) {
			return false;
		}
	}

	if (hasType) {
		if (meta.Type() != type) {
			return false;
		}
	}

	if (hasTime) {
		const auto t = meta.LastWriteTime();
		if (t < timeFrom || t > timeTo) {
			return false;
		}
	}

	if (hasSize) {
		const auto s = meta.Size();
		if (s < sizeMin || s > sizeMax) {
			return false;
		}
	}

	if (hasOwner) {
		if (!MatchString(meta.Owner(), ownerPattern, ownerMode)) {
			return false;
		}
	}

	return true;
}