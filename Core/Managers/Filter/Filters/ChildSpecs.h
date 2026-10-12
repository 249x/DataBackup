#pragma once

#include <string>
#include <vector>

/*
 * 复合条目（与/或）子条目的 spec 语法。
 *
 * 放在 Filters 里而不是管理器里：复合条目对管理器是**透明**的——管理器只管
 * "有哪些类型"和"登记了哪些条目"，括号与 ';' 怎么解释完全是复合条目自己的事。
 *
 *   and:(name:*.o;size:=0)                    最外层括号为可读性
 *   and:(name:*.o;or:(size:=0;name:*.obj))    括号内的 ';' 属于更内层
 *
 * 每个片段自己仍是 "名字:参数"（名字就是管理器类型表里的键），由 Parse 拆开。
 */
class ChildSpecs {
public:
	ChildSpecs() = delete;

	static std::vector<std::string> Split(const std::string& spec);
	static void Parse(const std::string& spec, std::string& typeName, std::string& parameters);

private:
	// 整串恰好被一层括号包住时去掉它们："(a;b)" -> "a;b"
	static std::string StripOuterParentheses(const std::string& text);
};
