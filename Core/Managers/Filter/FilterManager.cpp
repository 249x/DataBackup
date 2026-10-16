#include "FilterManager.h"

#include "Filters/AndFilter.h"
#include "Filters/NameFilter.h"
#include "Filters/NotFilter.h"
#include "Filters/OrFilter.h"
#include "Filters/OwnerFilter.h"
#include "Filters/PathFilter.h"
#include "Filters/SizeFilter.h"
#include "Filters/TimeFilter.h"
#include "Filters/TypeFilter.h"

#include "../Command/CommandManager.h"
#include "../../General/Debug.h"
#include "../../General/SerializationUtils.h"

#include <algorithm>
#include <iostream>
#include <utility>

template <typename T>
void FilterManager::RegisterFilter(FilterTypeInfo info) {
	RegisterType(std::move(info),
	             [](const FilterManager& owner, std::string name, std::uint32_t id)
	                 -> std::unique_ptr<Filter> {
		             return std::make_unique<T>(owner, std::move(name), id);
	             });
}

FilterManager::FilterManager(System& sys) : Manager(sys) {}

FilterManager::~FilterManager() = default;

void FilterManager::Initialize() {
	// 内置过滤类型的完整清单。登记名要与 Filter::Name() 是同一个串——它既用于错误提示，
	// 也是创建时冒号前的那个词（"name:*.tmp"）。
	//
	// 每个类型在登记时把界面信息一并填好（语法 / 示例 / 一句话说明），界面直接拿
	// FilterManager::TypeInfos() 去展示，不再自己抄一份。**顺序就是下面这个顺序**：
	// 界面的页签索引依赖它，别随便调换。语法细节写在各自头文件的类注释里；
	// 参数不合规会被过滤器自己记一条日志。
	RegisterFilter<NameFilter>({"name", "name:模式", "name:*.tmp",
	                            "按文件名匹配，* 和 ? 是通配符"});
	RegisterFilter<PathFilter>({"path", "path:模式", "path:src/*.csv",
	                            "按相对路径匹配，模式里统一写 /"});
	RegisterFilter<OwnerFilter>({"owner", "owner:模式", "owner:*admin*", "按文件属主账户名匹配"});
	RegisterFilter<TypeFilter>({"type", "type:类型", "type:regular,hardlink",
	                            "类型名用逗号分隔，任一命中即通过"});
	RegisterFilter<SizeFilter>({"size", "size:条件", "size:min=1M",
	                            "min= / max= / = / A..B，单位 k m g 按 1024"});
	RegisterFilter<TimeFilter>({"time", "time:条件", "time:min=2026-01-01",
	                            "min= / max= / A..B，可加 write / access / create 前缀"});
	RegisterFilter<AndFilter>({"and", "and:(子;子)", "and:(name:*.txt;size:min=1M)",
	                           "所有子条件都通过才通过"});
	RegisterFilter<OrFilter>({"or", "or:(子;子)", "or:(name:*.tmp;name:*.log)",
	                          "任一子条件通过即通过"});
	RegisterFilter<NotFilter>({"not", "not:(子)", "not:(name:*.tmp)", "子条件不通过才通过"});

	// 命令行入口：加（add-filter）、删（remove-filter）、改参数（set-filter）、看（filters）
	CommandManager* command = Get<CommandManager>();
	command->RegisterCommand("add-filter", "Add a filter: <spec>, e.g. name:*.tmp",
	                         [this](const std::string& spec) -> bool {
		                         const std::uint32_t id = CreateAndAdd(spec);
		                         if (id == 0) {
			                         return false; // 造不出来的原因 Create 已经记过日志
		                         }
		                         Debug::Info(
		                             "Filter #" + std::to_string(id) + " added: " + spec, LogTag);
		                         return true;
	                         });
	command->RegisterCommand("remove-filter", "Remove a filter: <id>",
	                         [this](std::uint32_t id) -> bool {
		                         if (!Remove(id)) {
			                         Debug::Error("No filter with id " + std::to_string(id), LogTag);
			                         return false;
		                         }
		                         Debug::Info("Filter #" + std::to_string(id) + " removed", LogTag);
		                         return true;
	                         });
	command->RegisterCommand("set-filter", "Change parameters of a filter: <id> <parameters>",
	                         SetParameters, this);
	command->RegisterCommand("filters", "Print all filters", PrintFilters, this);
	command->RegisterCommand("clear-filters", "Remove all filters", [this]() -> bool {
		const std::size_t count = Size();
		Clear();
		Debug::Info("Removed " + std::to_string(count) + " filter(s)", LogTag);
		return true;
	});
}

void FilterManager::ParseSpec(const std::string& spec, std::string& typeName,
                              std::string& parameters) {
	const std::size_t colon = spec.find(':');
	if (colon == std::string::npos) {
		typeName = spec;
		parameters.clear();
		return;
	}

	typeName = spec.substr(0, colon);
	parameters = spec.substr(colon + 1);
}

bool FilterManager::RegisterType(FilterTypeInfo info, Creator creator) {
	if (creator == nullptr) {
		Debug::Error("Filter creator is null", LogTag);
		return false;
	}
	if (info.name.empty()) {
		Debug::Error("Filter type name is empty", LogTag);
		return false;
	}
	if (FindType(info.name) != nullptr) {
		Debug::Warning("Filter type name already registered: " + info.name, LogTag);
		return false;
	}

	types.push_back(TypeEntry{std::move(info), creator});
	return true;
}

const FilterManager::TypeEntry* FilterManager::FindType(const std::string& typeName) const noexcept {
	for (const TypeEntry& entry : types) {
		if (entry.info.name == typeName) {
			return &entry;
		}
	}
	return nullptr;
}

bool FilterManager::IsTypeRegistered(const std::string& typeName) const noexcept {
	return FindType(typeName) != nullptr;
}

std::vector<FilterTypeInfo> FilterManager::TypeInfos() const {
	std::vector<FilterTypeInfo> infos;
	infos.reserve(types.size());
	for (const TypeEntry& entry : types) {
		infos.push_back(entry.info);
	}
	return infos;
}

std::string FilterManager::KnownTypeNames() const {
	std::string joined;
	for (const TypeEntry& entry : types) {
		if (!joined.empty()) {
			joined += ", ";
		}
		joined += entry.info.name;
	}
	return joined.empty() ? "(none)" : joined;
}

FilterManager::Creator FilterManager::CreatorOf(const std::string& typeName) const noexcept {
	const TypeEntry* entry = FindType(typeName);
	return entry != nullptr ? entry->creator : nullptr;
}

Filter* FilterManager::Build(const std::string& spec, std::uint32_t id) {
	std::string typeName;
	std::string parameters;
	ParseSpec(spec, typeName, parameters);

	const TypeEntry* entry = FindType(typeName);
	if (entry == nullptr) {
		Debug::Error("No filter type named '" + typeName + "'. Known: " + KnownTypeNames(), LogTag);
		return nullptr;
	}

	std::unique_ptr<Filter> filter = entry->creator(*this, typeName, id);
	if (!filter) {
		Debug::Error("Filter creator failed for type '" + typeName + "'", LogTag);
		return nullptr;
	}

	filter->SetParameters(parameters);

	Filter* raw = filter.get();
	filters.push_back(std::move(filter));
	return raw;
}

Filter* FilterManager::Create(const std::string& spec) {
	Filter* filter = Build(spec, nextId);
	if (filter != nullptr) {
		++nextId;
	}
	return filter;
}

std::uint32_t FilterManager::CreateAndAdd(const std::string& spec) {
	const Filter* filter = Create(spec);
	return filter != nullptr ? filter->Id() : 0;
}

bool FilterManager::AddWithId(std::uint32_t id, const std::string& spec) {
	if (Find(id) != nullptr) {
		Debug::Warning("Saved filter id " + std::to_string(id) + " is already taken, skipped",
		               LogTag);
		return false;
	}
	if (Build(spec, id) == nullptr) {
		return false; // 原因 Build 已经记过日志
	}
	if (nextId <= id) {
		nextId = id + 1; // 标识只增不减，从存档里恢复出来的也不例外
	}
	return true;
}

bool FilterManager::Remove(std::uint32_t id) {
	const auto it = std::find_if(filters.begin(), filters.end(),
	                             [id](const std::unique_ptr<Filter>& filter) {
		                             return filter && filter->Id() == id;
	                             });
	if (it == filters.end()) {
		return false;
	}

	filters.erase(it);
	return true;
}

void FilterManager::Clear() {
	filters.clear();
}

std::size_t FilterManager::Size() const noexcept {
	return filters.size();
}

const Filter* FilterManager::Find(std::uint32_t id) const noexcept {
	for (const std::unique_ptr<Filter>& filter : filters) {
		if (filter && filter->Id() == id) {
			return filter.get();
		}
	}
	return nullptr;
}

const Filter* FilterManager::At(std::size_t index) const noexcept {
	return index < filters.size() ? filters[index].get() : nullptr;
}

bool FilterManager::SetParameters(std::uint32_t id, const std::string& parameters) {
	const auto it = std::find_if(filters.begin(), filters.end(),
	                             [id](const std::unique_ptr<Filter>& filter) {
		                             return filter && filter->Id() == id;
	                             });
	if (it == filters.end()) {
		Debug::Error("No filter with id " + std::to_string(id), LogTag);
		return false;
	}

	Filter& filter = **it;
	const std::string oldSpec = filter.ToString();
	if (!filter.SetParameters(parameters)) {
		const std::size_t colon = oldSpec.find(':');
		filter.SetParameters(colon == std::string::npos ? std::string()
		                                               : oldSpec.substr(colon + 1));
		Debug::Warning("Filter #" + std::to_string(id) + " keeps '" + oldSpec + "'", LogTag);
		return false;
	}

	Debug::Info("Filter #" + std::to_string(id) + " -> " + filter.ToString(), LogTag);
	return true;
}

bool FilterManager::PrintFilters() {
	std::cout << "types (" << types.size() << "): " << KnownTypeNames() << "\n";
	if (filters.empty()) {
		std::cout << "filters (0): no constraint, every entry is backed up\n";
		return true;
	}

	std::cout << "filters (" << filters.size() << "), all of them must pass:\n";
	for (const std::unique_ptr<Filter>& filter : filters) {
		if (!filter) {
			continue;
		}
		std::cout << "  #" << filter->Id() << "  " << filter->ToString();
		if (!filter->IsValid()) {
			std::cout << "   (invalid: 参数没被理解，放行)";
		}
		std::cout << "\n";
	}
	return true;
}

bool FilterManager::Include(const FileMetaData& meta) const {
	for (const std::unique_ptr<Filter>& filter : filters) {
		if (filter && !filter->Check(meta)) {
			return false;
		}
	}
	return true;
}

bool FilterManager::Exclude(const FileMetaData& meta) const {
	for (const std::unique_ptr<Filter>& filter : filters) {
		if (filter && filter->Check(meta)) {
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------- 存档

bool FilterManager::Save(std::vector<std::uint8_t>& data) {
	data.clear();
	SerializationUtils io;

	std::uint32_t count = 0;
	for (const std::unique_ptr<Filter>& filter : filters) {
		if (filter) {
			++count;
		}
	}

	// 格式：nextId(4) + 条数(4) + 每条 [标识(4) + 规格串]
	if (!io.WriteUnsigned(data, nextId) || !io.WriteUnsigned(data, count)) {
		Debug::Error("Failed to serialize filters", LogTag);
		return false;
	}

	for (const std::unique_ptr<Filter>& filter : filters) {
		if (!filter) {
			continue;
		}
		// 存 ToString()（规范形式）：它由解析结果拼出来，所以存档里的规格与"实际判定"必然一致
		if (!io.WriteUnsigned(data, filter->Id()) || !io.WriteString(data, filter->ToString())) {
			Debug::Error("Failed to serialize filters", LogTag);
			return false;
		}
	}
	return true;
}

bool FilterManager::Load(const std::vector<std::uint8_t>& data) {
	SerializationUtils io;
	std::uint32_t savedNextId = 1;
	std::uint32_t count = 0;
	if (!io.ReadUnsigned(data, savedNextId) || !io.ReadUnsigned(data, count)) {
		Debug::Error("Saved filters are too short", LogTag);
		return false;
	}
	if (count > data.size()) {
		// 每条至少 8 字节，所以用条数当上界就能挡掉坏档引起的巨额分配
		Debug::Error("Saved filters are corrupt (" + std::to_string(count) + " entries)", LogTag);
		return false;
	}

	// 先把整段读完确认没坏，再落地：半途失败时原来的列表不会被改坏
	std::vector<std::pair<std::uint32_t, std::string>> entries;
	entries.reserve(count);
	for (std::uint32_t index = 0; index < count; ++index) {
		std::uint32_t id = 0;
		std::string spec;
		if (!io.ReadUnsigned(data, id) || !io.ReadString(data, spec)) {
			Debug::Error("Saved filters are truncated", LogTag);
			return false;
		}
		entries.emplace_back(id, std::move(spec));
	}

	Clear();
	nextId = savedNextId > 0 ? savedNextId : 1;

	for (const std::pair<std::uint32_t, std::string>& entry : entries) {
		AddWithId(entry.first, entry.second); // 恢复不了的条目自己会记日志，不打断其余的
	}
	return true;
}

std::uint32_t FilterManager::SaveID() const {
	return SaveIDValue;
}