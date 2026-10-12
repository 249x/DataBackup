#pragma once

#include "../Manager.h"
#include "../../FileStruct/StructuredFileContent.h"
#include "../../FileStruct/FileEntry.h"

#include <cstdint>
#include <filesystem>
#include <stack>
#include <vector>

class FilePipelineManager : public Manager {
public:
	FilePipelineManager(System& sys);
	~FilePipelineManager() override;

	void Initialize() override;

    bool Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath);
    bool Restore(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath);

	bool PrintSettings();

private:
	void FilterEntries(std::vector<FileEntry>& entries);

	bool PackEntries(std::vector<FileEntry>& entries);
	bool CompressEntries(std::vector<FileEntry>& entries, std::uint16_t type);
	bool EncryptEntries(std::vector<FileEntry>& entries, std::uint16_t type);

	bool DealArchive(const StructuredFileContent& content, std::stack<FileEntry>& stack);
	bool DealCompress(const StructuredFileContent& content, FileEntry& entry);
	bool DealEncrypt(const StructuredFileContent& content, FileEntry& entry);
};