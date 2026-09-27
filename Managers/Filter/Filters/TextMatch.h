#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

/*
 * 字符串过滤共用的字符操作：模式解释 + 按形状比较。全是静态方法，不持有状态。
 *
 * 名字 / 路径 / 属主三个过滤器的匹配规则完全一样，差别只有两点：
 *   1. 候选取自元数据的哪一项（各过滤器自己取，取的是视图，不分配）
 *   2. 是否额外认"分隔符等价"（路径过滤要认，文件名与属主不认）
 * 于是把"模式 -> 形状 + 字面串"与"按形状比较"都收在这里。
 *
 * 候选串的字符类型是模板参数：路径类用 path 的原生字符（Windows 上是 UTF-16），
 * 属主用 UTF-8 的 char。比较只折叠 ASCII 大小写。
 */
class TextMatch {
public:
	// 模式解释出来的形状，Matches 按它走各自最省的那条路
	enum class Shape {
		Exact,     // 全等：先比长度，再逐字符
		Prefix,    // 前缀
		Suffix,    // 后缀
		Contains,  // 子串：朴素搜索（名字/路径只有几十个字符，不值得上 KMP）
		Affix,     // 前缀 + 后缀："src/*.csv"：先卡长度再各比一次，不用扫描
		Glob,      // 通用通配：'*' 任意串、'?' 单字符，两个游标加一个回溯点
	};

	struct Options {
		// 文本里的 '\' 与字面串里的 '/' 视为相同（路径过滤用：Windows 两种分隔符都会出现）
		bool separatorsEqual = false;
	};

	TextMatch() = delete;

	// ---- UTF-8 模式 -> 已小写化的字面串 ----
	// 路径类：转成 path 的原生字符（转码只做这一次，Match 里就没有它了）
	static std::filesystem::path::string_type NativeLiteral(const std::string& utf8);
	// 属主类：它本身就是 UTF-8，只小写化
	static std::string LowerLiteral(const std::string& utf8);
	// 去掉首尾 ASCII 空白
	static std::string Trim(const std::string& text);

	// 把模式解释成"形状 + 最多两段字面串"（仍是 UTF-8 片段，由调用方按自己的字符类型转换）。
	// 形状推断只看 '*' '?'，它们在 UTF-8 与 UTF-16 里都是单字符，所以在 UTF-8 上做没问题。
	// 模式为空（或全空白）时返回 false，调用方据此把条目标成无效
	static bool Split(const std::string& pattern, Shape& shape, std::string& head,
	                  std::string& tail);

	// 按形状比较。text 是候选串的视图，head/tail 必须是已小写化的字面串；
	// 只有 Affix 会用 tail，其余形状忽略它。字符类型由调用方显式给出
	template <typename CharT>
	static bool Matches(Shape shape, std::basic_string_view<CharT> text,
	                    std::basic_string_view<CharT> head, std::basic_string_view<CharT> tail,
	                    Options options = Options());

	// 只折叠 ASCII 大小写（其余字节原样，含 UTF-8 的多字节部分）
	template <typename CharT>
	static CharT LowerAscii(CharT c) noexcept;

	// '/' 与 '\'：Windows 上两个都算分隔符，其它平台只算 '/'
	template <typename CharT>
	static bool IsSeparator(CharT c) noexcept;

private:
	// 文本字符与字面字符是否算相同：大小写之外，按 options 额外认分隔符等价
	template <typename CharT>
	static bool SameChar(CharT text, CharT literal, Options options) noexcept;
	template <typename CharT>
	static bool StartsWith(std::basic_string_view<CharT> text,
	                       std::basic_string_view<CharT> literal, Options options);
	template <typename CharT>
	static bool EndsWith(std::basic_string_view<CharT> text,
	                     std::basic_string_view<CharT> literal, Options options);
	template <typename CharT>
	static bool Contains(std::basic_string_view<CharT> text,
	                     std::basic_string_view<CharT> literal, Options options);
	template <typename CharT>
	static bool GlobMatch(std::basic_string_view<CharT> text,
	                      std::basic_string_view<CharT> pattern, Options options);
};

// ---- 模板实现 ----

template <typename CharT>
CharT TextMatch::LowerAscii(CharT c) noexcept {
	if (c >= 'A' && c <= 'Z') {
		return static_cast<CharT>(c - 'A' + 'a');
	}
	return c;
}

template <typename CharT>
bool TextMatch::IsSeparator(CharT c) noexcept {
	if (c == static_cast<CharT>('/')) {
		return true;
	}
#if defined(_WIN32)
	return c == static_cast<CharT>('\\');
#else
	return false;
#endif
}

template <typename CharT>
bool TextMatch::SameChar(CharT text, CharT literal, Options options) noexcept {
	if (LowerAscii(text) == literal) {
		return true;
	}
	return options.separatorsEqual && literal == static_cast<CharT>('/') && IsSeparator(text);
}

template <typename CharT>
bool TextMatch::StartsWith(std::basic_string_view<CharT> text,
                           std::basic_string_view<CharT> literal, Options options) {
	if (text.size() < literal.size()) {
		return false;
	}
	for (std::size_t i = 0; i < literal.size(); ++i) {
		if (!SameChar(text[i], literal[i], options)) {
			return false;
		}
	}
	return true;
}

template <typename CharT>
bool TextMatch::EndsWith(std::basic_string_view<CharT> text,
                         std::basic_string_view<CharT> literal, Options options) {
	if (text.size() < literal.size()) {
		return false;
	}
	const std::size_t offset = text.size() - literal.size();
	for (std::size_t i = 0; i < literal.size(); ++i) {
		if (!SameChar(text[offset + i], literal[i], options)) {
			return false;
		}
	}
	return true;
}

template <typename CharT>
bool TextMatch::Contains(std::basic_string_view<CharT> text,
                         std::basic_string_view<CharT> literal, Options options) {
	if (literal.empty()) {
		return true;
	}
	if (text.size() < literal.size()) {
		return false;
	}

	// 朴素搜索：先比首字符再比后续，文件名/路径只有几十个字符，实测不值得上 KMP
	const std::size_t last = text.size() - literal.size();
	for (std::size_t at = 0; at <= last; ++at) {
		if (!SameChar(text[at], literal[0], options)) {
			continue;
		}
		std::size_t i = 1;
		while (i < literal.size() && SameChar(text[at + i], literal[i], options)) {
			++i;
		}
		if (i == literal.size()) {
			return true;
		}
	}
	return false;
}

template <typename CharT>
bool TextMatch::GlobMatch(std::basic_string_view<CharT> text,
                          std::basic_string_view<CharT> pattern, Options options) {
	std::size_t t = 0;
	std::size_t p = 0;
	std::size_t starAt = std::basic_string_view<CharT>::npos;
	std::size_t resume = 0;

	while (t < text.size()) {
		if (p < pattern.size() &&
		    (pattern[p] == static_cast<CharT>('?') || SameChar(text[t], pattern[p], options))) {
			++t;
			++p;
		} else if (p < pattern.size() && pattern[p] == static_cast<CharT>('*')) {
			starAt = p++;
			resume = t;
		} else if (starAt != std::basic_string_view<CharT>::npos) {
			// 回溯到上一个 '*'，让它多吞一个字符
			p = starAt + 1;
			t = ++resume;
		} else {
			return false;
		}
	}
	// 收尾：模式剩下的只能是 '*'
	while (p < pattern.size() && pattern[p] == static_cast<CharT>('*')) {
		++p;
	}
	return p == pattern.size();
}

template <typename CharT>
bool TextMatch::Matches(Shape shape, std::basic_string_view<CharT> text,
                        std::basic_string_view<CharT> head, std::basic_string_view<CharT> tail,
                        Options options) {
	switch (shape) {
	case Shape::Exact:
		return text.size() == head.size() && StartsWith(text, head, options);
	case Shape::Prefix:
		return StartsWith(text, head, options);
	case Shape::Suffix:
		return EndsWith(text, head, options);
	case Shape::Contains:
		return Contains(text, head, options);
	case Shape::Affix:
		return text.size() >= head.size() + tail.size() && StartsWith(text, head, options) &&
		       EndsWith(text, tail, options);
	case Shape::Glob:
		return GlobMatch(text, head, options);
	}
	return false;
}
