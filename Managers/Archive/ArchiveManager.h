#pragma once

#include "../Manager.h"
#include "../../FileStruct/FileEntry.h"

#include <filesystem>
#include <vector>

class ArchiveManager : public Manager {
public:
	ArchiveManager(System& sys);
	~ArchiveManager() override;

	void Initialize() override;

	bool Pack(const std::vector<FileEntry>& entries, std::uint16_t& fileCount, std::vector<uint8_t>& archive) const;
	bool Unpack(const std::vector<uint8_t>& archive, const std::uint16_t fileCount, std::vector<FileEntry>& entries) const;
	

};
