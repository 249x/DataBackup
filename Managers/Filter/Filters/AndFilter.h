#pragma once

#include "Filter.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/*
 * "与"：所有子条目都通过才算通过。
 *
 *   and:(name:*.o;size:=0)                  两个条件都满足
 *   and:(name:*.o;or:(size:=0;name:*.obj))  可以嵌套
 *
 * 子条目在 ParseParameters 里造：参数串按深度 0 的 ';' 切成若干 spec，逐个交给
 * Manager().CreatorOf(name) 拿到的创建函数 —— 子条目由本条目自己持有（children 字段），
 * 不进管理器的实例表（标识为 0），所以复合可以任意嵌套。
 */
class AndFilter : public Filter {
public:
	AndFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;

	std::vector<std::unique_ptr<Filter>> children;
};
