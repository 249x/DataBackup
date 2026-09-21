#pragma once

#include "../Manager.h"
#include "../../FileStruct/StructuredFileContent.h"
#include "../../FileStruct/FileEntry.h"

#include <cstdint>
#include <filesystem>
#include <stack>
#include <vector>

struct BackupSettings {
	bool archive = true;
	bool compress = false;
	std::uint16_t compressType = 0;
	bool encrypt = false;
	std::uint16_t encryptType = 0;
};

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

	bool SetArchive(bool enabled);
	bool SetCompression(bool enabled, std::uint16_t type);
	bool SetEncryption(bool enabled, std::uint16_t type);
	bool PrintSettings();

	const BackupSettings& Settings() const noexcept;

private:
	// 把转换结果写出去：单条目写成一个文件，多条目才写成一棵树
	bool WriteResult(const std::filesystem::path& tarPath,
	                 const std::vector<FileEntry>& entries);
	bool PackEntries(std::vector<FileEntry>& entries);
	bool CompressEntries(std::vector<FileEntry>& entries, std::uint16_t type);
	bool EncryptEntries(std::vector<FileEntry>& entries, std::uint16_t type);

	std::vector<uint8_t> key;
	BackupSettings settings;

	bool DealArchive(const StructuredFileContent& content, std::stack<FileEntry>& stack);
	bool DealCompress(const StructuredFileContent& content, FileEntry& entry);
	bool DealEncrypt(const StructuredFileContent& content, FileEntry& entry);
};