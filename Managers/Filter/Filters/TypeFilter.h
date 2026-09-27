#pragma once

#include "Filter.h"

#include "../../../FileStruct/FileType.h"

#include <cstdint>
#include <string>

/*
 * 按**文件类型**过滤，名字取自 FileTypeName（FileType.h），不区分大小写：
 *
 *   type:regular              普通文件
 *   type:directory            目录
 *   type:regular hardlink     普通文件或硬链接，逗号或空白分隔都行
 *
 * ParseParameters 里把选中的类型折成一个位图，Match 只需一次与运算。
 */
class TypeFilter : public Filter {
public:
	TypeFilter(const FilterManager& manager, std::string name, std::uint32_t id);

	std::string ToString() const override;

private:
	bool ParseParameters(const std::string parameters) override;
	bool Match(const FileMetaData& meta) const override;

	// FileType 取值 0..10，用位图判"是不是这些类型之一"最快
	static constexpr unsigned BitCount = 16;

	// 把 "regular,hardlink" 这样的文本折成位图；有认不出的名字、或一个名字都没有时返回 false
	static bool ParseNames(const std::string& text, std::uint16_t& mask);

	std::uint16_t mask = 0;
};
