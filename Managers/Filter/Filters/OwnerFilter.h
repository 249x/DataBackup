#pragma once

#include "Filter.h"
#include "TextMatch.h"

#include <cstdint>
#include <string>

/*
 * 按**属主**（FileMetaData::Owner()，账户名）过滤。
 *
 * 模式里只有 '*' 和 '?' 是元字符，其余字符都是普通字符：
 *
 *   owner:alice              全等
 *   owner:*alice*            子串 "alice"
 *   owner:CORP\*             前缀 "CORP\"
 *   owner:*admin*            子串 "admin"（域账户常见的写法）
 *
 * 比较忽略 ASCII 大小写；非 ASCII 字节按原样比较（账户名里出现的非 ASCII 极少）。
 *
 * 属主本身就是 UTF-8 字符串，所以字面串也留 UTF-8：Match 直接拿 Owner() 比，
 * 连转码都不需要（TextMatch 只负责解释模式与比较）。
 */
class OwnerFilter : public Filter {
public:
	OwnerFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;

	TextMatch::Shape shape = TextMatch::Shape::Exact;
	// 已小写化的字面串（UTF-8）；Affix 时 head 是前缀、tail 是后缀
	std::string head;
	std::string tail;
};
