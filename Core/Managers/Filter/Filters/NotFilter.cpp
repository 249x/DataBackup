#include "NotFilter.h"

#include "../FilterManager.h"
#include "ChildSpecs.h"

#include "../../../General/Debug.h"

#include <utility>
#include <vector>

NotFilter::NotFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool NotFilter::ParseParameters(const std::string parameters) {
	child.reset();

	// 括号与 ';' 的解释、子条目的创建都在本类里做完，管理器只知道"有哪些类型"
	const std::vector<std::string> specs = ChildSpecs::Split(parameters);
	if (specs.size() != 1) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": expects exactly one child (expected "
		             "'not:(子条目)'), got " + std::to_string(specs.size()),
		             LogTag);
		return false;
	}

	const std::string& spec = specs.front();
	std::string childName;
	std::string childParameters;
	ChildSpecs::Parse(spec, childName, childParameters);

	const FilterManager::Creator creator = Manager().CreatorOf(childName);
	std::unique_ptr<Filter> candidate =
	    creator != nullptr ? creator(Manager(), childName, 0) : nullptr;
	if (!candidate) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": can't make child '" + spec + "'", LogTag);
		return false;
	}

	candidate->SetParameters(childParameters);

	// 取反会把"子条目无效就放行"翻转成"什么都不放行"。宁可当作没这条约束，也不能让备份变空
	if (!candidate->IsValid()) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) + ": child '" + spec +
		                 "' has unusable parameters, this 'not' is dropped",
		             LogTag);
		return false;
	}

	child = std::move(candidate);
	SetValid(true);
	return true;
}

bool NotFilter::Match(const FileMetaData& meta) const {
	if (!child) {
		return true; // 走不到：Check 会先拦住无效条目
	}
	return !child->Check(meta);
}

std::string NotFilter::ToString() const {
	return child ? Name() + ":(" + child->ToString() + ")" : Name() + ":()";
}
