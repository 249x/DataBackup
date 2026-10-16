#pragma once

#include "../Manager.h"
#include "../Save/ISaveData.h"

#include "FilterTypeInfo.h"
#include "Filters/Filter.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class FilterManager : public Manager, public ISaveData {
public:
	using Creator = std::unique_ptr<Filter> (*)(const FilterManager& manager, std::string name,
	                                            std::uint32_t id);

	FilterManager(System& sys);
	~FilterManager() override;

	void Initialize() override;

	bool RegisterType(FilterTypeInfo info, Creator creator);
	bool IsTypeRegistered(const std::string& typeName) const noexcept;

	std::vector<FilterTypeInfo> TypeInfos() const;

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

	// ---- ISaveData：筛选条件跟着存档一起活下来 ----
	// 这个标识会写进 save.dat，必须保持稳定
	static constexpr std::uint32_t SaveIDValue = 2;
	bool Save(std::vector<std::uint8_t>& data) override;
	bool Load(const std::vector<std::uint8_t>& data) override;
	std::uint32_t SaveID() const override;

	bool Include(const FileMetaData& meta) const;
	bool Exclude(const FileMetaData& meta) const;

private:
	struct TypeEntry {
		FilterTypeInfo info;
		Creator creator;
	};

	static void ParseSpec(const std::string& spec, std::string& typeName, std::string& parameters);
	std::string KnownTypeNames() const;
	const TypeEntry* FindType(const std::string& typeName) const noexcept;
	// 按 "类型:参数" 造一个条目并放进实例表（标识由调用方给）；造不出来返回 nullptr
	Filter* Build(const std::string& spec, std::uint32_t id);
	// 恢复存档里的条目：标识照原样装回去，并把 nextId 顶到它之后（标识只增不减）
	bool AddWithId(std::uint32_t id, const std::string& spec);
	template <typename T>
	void RegisterFilter(FilterTypeInfo info);

	static constexpr const char* LogTag = "Filter";
	std::vector<TypeEntry> types;                    // 类型表，按注册顺序
	std::vector<std::unique_ptr<Filter>> filters;    // 实例表（线性），按加入顺序
	std::uint32_t nextId = 1;                        // 下一个要分配的标识（只增不减）
};
