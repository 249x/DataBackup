#include "AndFilter.h"

#include "../FilterManager.h"
#include "ChildSpecs.h"

#include "../../../General/Debug.h"

#include <cstddef>
#include <utility>

AndFilter::AndFilter(const FilterManager& manager, std::string name, std::uint32_t id)
	: Filter(manager, std::move(name), id) {}

bool AndFilter::ParseParameters(const std::string parameters) {
	children.clear();

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

		child->SetParameters(childParameters);
		children.push_back(std::move(child));
	}

	if (children.empty()) {
		SetValid(false);
		Debug::Error("Filter #" + std::to_string(Id()) +
		                 ": no child entry (expected 'and:(子条目;子条目)')",
		             LogTag);
		return false;
	}

	SetValid(true);
	return true;
}

bool AndFilter::Match(const FileMetaData& meta) const {
	for (const std::unique_ptr<Filter>& child : children) {
		if (!child->Check(meta)) {
			return false;
		}
	}
	return true;
}

std::string AndFilter::ToString() const {
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
