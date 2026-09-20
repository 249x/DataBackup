#pragma once

#include "../Manager.h"
#include "../../FileStruct/StructuredFileContent.h"
#include "../../FileStruct/FileEntry.h"

#include <filesystem>
#include <stack>


class FilePipelineManager : public Manager {
public:
	FilePipelineManager(System& sys);
	~FilePipelineManager() override;

	void Initialize() override;

    bool Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath);
	bool Archive(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath);
	bool Compress(const std::filesystem::path& srcPath,
                const std::filesystem::path& tarPath,
                const ::uint16_t& type);
	bool Encrypt(const std::filesystem::path& srcPath,
                const std::filesystem::path& tarPath,
                const ::uint16_t& type);
    bool Restore(const std::filesystem::path& srcPath, 
		const std::filesystem::path& tarPath);

	bool SetKeyPath(const std::filesystem::path& keyPath);

private:
	std::vector<uint8_t> key;
	bool DealArchive(const StructuredFileContent& content, std::stack<FileEntry>& stack);
	bool DealCompress(const StructuredFileContent& content, FileEntry& entry);
	bool DealEncrypt(const StructuredFileContent& content, FileEntry& entry);
};