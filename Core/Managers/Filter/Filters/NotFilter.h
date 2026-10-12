#pragma once

#include "Filter.h"

#include <cstdint>
#include <memory>
#include <string>

//
// "非"：唯一子条目不通过才算通过。
//
//   not:(name:*.txt)        不是 .txt
//   not:(not:(name:*.txt))  可以套，双层取反
//
// 只收**一个**子条目：一个都没给、或者给多了 → 整条无效。
//
// 子条目参数没被理解时，这条 not 自己也标成无效
//
class NotFilter : public Filter {
public:
	NotFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;

	std::unique_ptr<Filter> child;
};
