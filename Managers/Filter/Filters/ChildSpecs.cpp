#include "ChildSpecs.h"

#include "TextMatch.h"

#include "../../../General/Debug.h"

#include <cstddef>

namespace {

constexpr const char* LogTag = "Filter";

} // namespace

std::string ChildSpecs::StripOuterParentheses(const std::string& text) {
	if (text.size() < 2 || text.front() != '(' || text.back() != ')') {
		return text;
	}

	int depth = 0;
	for (std::size_t i = 0; i < text.size(); ++i) {
		if (text[i] == '(') {
			++depth;
		} else if (text[i] == ')') {
			--depth;
			if (depth == 0) {
				// 第一层括号在末尾才闭合，说明它包住了整串
				return i + 1 == text.size() ? text.substr(1, text.size() - 2) : text;
			}
		}
	}
	return text;
}

std::vector<std::string> ChildSpecs::Split(const std::string& spec) {
	std::vector<std::string> pieces;

	// 外层那对括号只为了可读，先脱掉；空串、全空白都返回空表
	const std::string body = StripOuterParentheses(TextMatch::Trim(spec));

	std::size_t start = 0;
	int depth = 0;
	for (std::size_t i = 0; i <= body.size(); ++i) {
		const bool atEnd = (i == body.size());
		char current = ';';
		if (!atEnd) {
			current = body[i];
			if (current == '(') {
				++depth;
			} else if (current == ')') {
				--depth;
				if (depth < 0) {
					Debug::Error("Unbalanced ')' in filter spec: " + spec, LogTag);
					depth = 0;
				}
			}
		}

		// 末尾一定要冲刷：即使括号不配对（depth != 0）也不能把尾巴整段丢掉
		if (atEnd || (current == ';' && depth == 0)) {
			const std::string piece = TextMatch::Trim(body.substr(start, i - start));
			if (!piece.empty()) {
				pieces.push_back(piece);
			}
			start = i + 1;
		}
	}

	if (depth != 0) {
		Debug::Error("Unbalanced '(' in filter spec: " + spec, LogTag);
	}
	return pieces;
}

void ChildSpecs::Parse(const std::string& spec, std::string& typeName, std::string& parameters) {
	const std::size_t colon = spec.find(':');
	if (colon == std::string::npos) {
		typeName = spec;
		parameters.clear();
		return;
	}

	typeName = spec.substr(0, colon);
	parameters = spec.substr(colon + 1);
}
