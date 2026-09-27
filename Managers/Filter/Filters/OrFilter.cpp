#include "OrFilter.h"

#include "../FilterManager.h"
#include "ChildSpecs.h"

#include "../../../General/Debug.h"

#include <cstddef>
#include <utility>

OrFilter::OrFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool OrFilter::ParseParameters(const std::string parameters) {
	children.clear();

	// 括号与 ';' 的解释、子条目的创建都在本类里做完：管理器只知道"有哪些类型"，
	// 不为复合条目提供任何专门入口
	for (const std::string& spec : ChildSpecs::Split(parameters)) {
		std::string childName;
		std::string childParameters;
		ChildSpecs::Parse(spec, childName, childParameters);

		const FilterManager::Creator creator = Manager().CreatorOf(childName);
		std::unique_ptr<Filter> child =
		    creator != nullptr ? creator(Manager(), childName, 0) : nullptr;
		if (!child) {
			children.clear();
			SetValid(false);
			Debug::Error("Filter #" + std::to_string(Id()) + ": can't make child '" + spec + "'",
			             LogTag);
			return false;
		}

		// 子条目由本条目自己持有（标识给 0），不进管理器的实例表
		child->SetParameters(childParameters);
		children.push_back(std::move(child));
	}

	// 一个子条目都没有（"or:" 这种少写了内容的）同样按无效处理，不让它静默通过
	if (children.empty()) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) +
		                 ": no child entry (expected 'or:(子条目;子条目)')",
		             LogTag);
		return false;
	}

	SetValid(true);
	return true;
}

bool OrFilter::Match(const FileMetaData& meta) const {
	for (const std::unique_ptr<Filter>& child : children) {
		if (child->Check(meta)) {
			return true; // 有一个通过就整体通过
		}
	}
	return false;
}

std::string OrFilter::ToString() const {
	std::string text = Name() + ":(";
	for (std::size_t i = 0; i < children.size(); ++i) {
		if (i > 0) {
			text += ";";
		}
		text += children[i]->ToString();
	}
	text += ")";
	return text;
}
