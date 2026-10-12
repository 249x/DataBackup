#include "TextMatch.h"

#include "../../../General/Utf8.h"

#include <algorithm>
#include <cctype>

std::filesystem::path::string_type TextMatch::NativeLiteral(const std::string& utf8) {
	std::filesystem::path::string_type text = Utf8::ToPath(utf8).native();
	for (auto& c : text) {
		c = LowerAscii(c);
	}
	return text;
}

std::string TextMatch::LowerLiteral(const std::string& utf8) {
	std::string text = utf8;
	for (char& c : text) {
		c = LowerAscii(c);
	}
	return text;
}

std::string TextMatch::Trim(const std::string& text) {
	const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
	const auto first = std::find_if(text.begin(), text.end(), notSpace);
	if (first == text.end()) {
		return {};
	}
	const auto last = std::find_if(text.rbegin(), text.rend(), notSpace).base();
	return std::string(first, last);
}

bool TextMatch::Split(const std::string& pattern, Shape& shape, std::string& head,
                      std::string& tail) {
	shape = Shape::Exact;
	head.clear();
	tail.clear();

	const std::string trimmed = Trim(pattern);
	if (trimmed.empty()) {
		return false;
	}

	// 没有元字符：整串就是字面串（'_' 之类一律当普通字符）
	if (trimmed.find_first_of("*?") == std::string::npos) {
		head = trimmed;
		shape = Shape::Exact;
		return true;
	}

	// 有元字符：先把两端连续的 '*' 掐掉，剩下的部分决定能不能走更省的路
	std::string body = trimmed;
	bool leadStar = false;
	bool trailStar = false;
	while (!body.empty() && body.front() == '*') {
		leadStar = true;
		body.erase(body.begin());
	}
	while (!body.empty() && body.back() == '*') {
		trailStar = true;
		body.pop_back();
	}

	const bool hasQuestion = body.find('?') != std::string::npos;
	const std::size_t stars = static_cast<std::size_t>(std::count(body.begin(), body.end(), '*'));

	if (!hasQuestion && stars == 0 && !body.empty()) {
		// 通配只在两端：就是 前缀 / 后缀 / 子串，比逐字符通配快得多
		if (leadStar && trailStar) {
			shape = Shape::Contains;
		} else if (leadStar) {
			shape = Shape::Suffix;
		} else {
			shape = Shape::Prefix;
		}
		head = body;
		return true;
	}

	if (!hasQuestion && stars == 1 && !leadStar && !trailStar) {
		// 中间恰好一个 '*'（"src/*.csv"）：前缀 + 后缀，长度卡住后各比一次
		const std::size_t star = body.find('*');
		const std::string left = body.substr(0, star);
		const std::string right = body.substr(star + 1);
		if (!left.empty() && !right.empty()) {
			shape = Shape::Affix;
			head = left;
			tail = right;
			return true;
		}
	}

	// '?' 或多个 '*'：通用通配（"*" 自己也在这一支，它匹配一切）
	shape = Shape::Glob;
	head = trimmed;
	return true;
}
