#pragma once

#include "Filter.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

//
// "或"：任一子条目通过就算通过。
//
//   or:(name:*.o;name:*.obj)                两种扩展名都算
//   or:(and:(name:*.o;size:=0);path:src/*)  可以嵌套（这条用行注释写，是因为
//                                           "路径 + '/' + '*'" 会把块注释提前结束）
//
// 子条目在 ParseParameters 里造：参数串按深度 0 的 ';' 切成若干 spec，逐个交给
// Manager().CreatorOf(name) 拿到的创建函数 —— 子条目由本条目自己持有（children 字段），
// **不进**管理器的实例表（标识为0），所以复合可以任意嵌套。
//
class OrFilter : public Filter {
public:
	OrFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;

	std::vector<std::unique_ptr<Filter>> children;
};
