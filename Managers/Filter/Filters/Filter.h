#pragma once

#include "../../../FileStruct/FileMetaData.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class FilterManager;
class Filter {
public:
	Filter(const FilterManager& manager, std::string name, std::uint32_t id);
	virtual ~Filter();

	const std::string& Name() const noexcept;
	std::uint32_t Id() const noexcept;
	bool Check(const FileMetaData& meta) const;


	bool SetParameters(const std::string parameters);
	virtual std::string ToString() const = 0;
	
	bool IsValid() const noexcept;
	void SetValid(bool valid) noexcept;
protected:
	static constexpr const char* LogTag = "Filter";

	const FilterManager& Manager() const noexcept;
	const std::string& RawParameters() const noexcept;

	virtual bool ParseParameters(const std::string parameters) = 0;
	virtual bool Match(const FileMetaData& meta) const = 0;
	
private:
	const FilterManager* manager;
	std::string name;
	std::string rawParameters;
	std::uint32_t id = 0;
	bool valid = false;
};
