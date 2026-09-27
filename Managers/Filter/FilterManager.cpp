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

#include <algorithm>
#include <iostream>
#include <utility>

template <typename T>
void FilterManager::RegisterFilter(const std::string& typeName) {
	RegisterType(typeName,
	             [](const FilterManager& owner, std::string name,
	                std::uint32_t id) -> std::unique_ptr<Filter> {
		             return std::make_unique<T>(owner, std::move(name), id);
	             });
}

FilterManager::FilterManager(System& sys) : Manager(sys) {}

FilterManager::~FilterManager() = default;

void FilterManager::Initialize() {
	// 内置过滤类型的完整清单。登记名要与 Filter::Name() 是同一个串——它既用于错误提示，
	// 也是创建时冒号前的那个词（"name:*.tmp"）。
	//
	//   name    文件名（不含目录）：'*' '?' 是元字符，其余（含 '_'）都是普通字符
	//   path    条目相对路径：同上，模式里统一写 '/'，文本里的 '\' 也算相同
	//   owner   属主账户名：同上
	//   type    类型名：regular / directory / symlink,junction …（多个取任一）
	//   size    min=1M / max=4K / =0 / 1M..2M（单位 k/m/g，二进制）
	//   time    min=2024-01-01 / max=2024-12-31 / A..B（可带 write|access|create 前缀）
	//   and     and:(子;子) / and:子;子    子条目全通过才算通过
	//   or      or:(子;子)                 任一子条目通过就算通过
	//   not     not:(子)                   唯一子条目不通过才算通过
	//
	// 语法细节写在各自头文件的类注释里；参数不合规会被过滤器自己记一条日志
	RegisterFilter<NameFilter>("name");
	RegisterFilter<PathFilter>("path");
	RegisterFilter<OwnerFilter>("owner");
	RegisterFilter<TypeFilter>("type");
	RegisterFilter<SizeFilter>("size");
	RegisterFilter<TimeFilter>("time");
	RegisterFilter<AndFilter>("and");
	RegisterFilter<OrFilter>("or");
	RegisterFilter<NotFilter>("not");

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

bool FilterManager::RegisterType(std::string typeName, Creator creator) {
	if (creator == nullptr) {
		Debug::Error("Filter creator is null", LogTag);
		return false;
	}
	if (typeName.empty()) {
		Debug::Error("Filter type name is empty", LogTag);
		return false;
	}
	if (creators.find(typeName) != creators.end()) {
		Debug::Warning("Filter type name already registered: " + typeName, LogTag);
		return false;
	}

	creators.emplace(std::move(typeName), creator);
	return true;
}

bool FilterManager::IsTypeRegistered(const std::string& typeName) const noexcept {
	return creators.find(typeName) != creators.end();
}

std::string FilterManager::KnownTypeNames() const {
	std::string joined;
	for (const auto& pair : creators) {
		if (!joined.empty()) {
			joined += ", ";
		}
		joined += pair.first;
	}
	return joined.empty() ? "(none)" : joined;
}

FilterManager::Creator FilterManager::CreatorOf(const std::string& typeName) const noexcept {
	const auto it = creators.find(typeName);
	return it == creators.end() ? nullptr : it->second;
}

Filter* FilterManager::Create(const std::string& spec) {
	std::string typeName;
	std::string parameters;
	ParseSpec(spec, typeName, parameters);

	const auto it = creators.find(typeName);
	if (it == creators.end()) {
		Debug::Error("No filter type named '" + typeName + "'. Known: " + KnownTypeNames(), LogTag);
		return nullptr;
	}

	std::unique_ptr<Filter> filter = it->second(*this, typeName, nextId);
	if (!filter) {
		Debug::Error("Filter creator failed for type '" + typeName + "'", LogTag);
		return nullptr;
	}

	filter->SetParameters(parameters);

	++nextId;
	Filter* raw = filter.get();
	filters.push_back(std::move(filter));
	return raw;
}

std::uint32_t FilterManager::CreateAndAdd(const std::string& spec) {
	const Filter* filter = Create(spec);
	return filter != nullptr ? filter->Id() : 0;
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
	std::cout << "types (" << creators.size() << "): " << KnownTypeNames() << "\n";
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