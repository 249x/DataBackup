#pragma once

#include "../Manager.h"

#include "Filters/Filter.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class FilterManager : public Manager {
public:
	using Creator = std::unique_ptr<Filter> (*)(const FilterManager& manager, std::string name,
	                                            std::uint32_t id);

	FilterManager(System& sys);
	~FilterManager() override;

	void Initialize() override;

	bool RegisterType(std::string typeName, Creator creator);
	bool IsTypeRegistered(const std::string& typeName) const noexcept;

	Filter* Create(const std::string& spec);
	std::uint32_t CreateAndAdd(const std::string& spec);

	bool SetParameters(std::uint32_t id, const std::string& parameters);

	Creator CreatorOf(const std::string& typeName) const noexcept;

	bool Remove(std::uint32_t id);
	void Clear();
	std::size_t Size() const noexcept;
	const Filter* Find(std::uint32_t id) const noexcept;
	const Filter* At(std::size_t index) const noexcept;

	bool PrintFilters();

	bool Include(const FileMetaData& meta) const;
	bool Exclude(const FileMetaData& meta) const;

private:
	static void ParseSpec(const std::string& spec, std::string& typeName, std::string& parameters);
	std::string KnownTypeNames() const;
	template <typename T>
	void RegisterFilter(const std::string& typeName);

	static constexpr const char* LogTag = "Filter";
	std::map<std::string, Creator> creators;         // 类型表：名字 -> 创建函数
	std::vector<std::unique_ptr<Filter>> filters;    // 实例表（线性），按加入顺序
	std::uint32_t nextId = 1;                        // 下一个要分配的标识（只增不减）
};
