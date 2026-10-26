#pragma once

#include "../Manager.h"
#include "../Save/ISaveData.h"
#include "../../FileStruct/StructuredFileContent.h"
#include "../../FileStruct/FileEntry.h"

#include <cstdint>
#include <filesystem>
#include <stack>
#include <vector>

class FilePipelineManager : public Manager, public ISaveData {
public:
	FilePipelineManager(System& sys);
	~FilePipelineManager() override;

	void Initialize() override;

    bool SetBackupPaths(const std::filesystem::path& source,
                        const std::filesystem::path& destination);
    bool SetRestorePaths(const std::filesystem::path& source,
                         const std::filesystem::path& destination);

    const std::filesystem::path& BackupSource() const noexcept;
    const std::filesystem::path& BackupDestination() const noexcept;
    const std::filesystem::path& RestoreSource() const noexcept;
    const std::filesystem::path& RestoreDestination() const noexcept;

    bool Backup();
    bool Restore();

	bool SetArchive(bool enabled);
	bool SetCompression(bool enabled, std::uint16_t type);
	bool SetEncryption(bool enabled, std::uint16_t type);

	bool ArchiveEnabled() const noexcept;
	bool CompressionEnabled() const noexcept;
	bool EncryptionEnabled() const noexcept;
	std::uint16_t CompressionType() const noexcept;
	std::uint16_t EncryptionType() const noexcept;

	bool PrintSettings();

	static constexpr std::uint32_t SaveIDValue = 1;
	bool Save(std::vector<std::uint8_t>& data) override;
	bool Load(const std::vector<std::uint8_t>& data) override;
	std::uint32_t SaveID() const override;

private:
	static constexpr const char* LogInfo = "Pipeline";
	void FilterEntries(std::vector<FileEntry>& entries);

	static std::string Where(const FileEntry& entry);

	bool PackEntries(std::vector<FileEntry>& entries);
	bool CompressEntries(std::vector<FileEntry>& entries, std::uint16_t type);
	bool EncryptEntries(std::vector<FileEntry>& entries, std::uint16_t type);

	bool DealArchive(const StructuredFileContent& content, std::stack<FileEntry>& stack);
	bool DealCompress(const StructuredFileContent& content, FileEntry& entry);
	bool DealEncrypt(const StructuredFileContent& content, FileEntry& entry);

	bool archiveEnabled = true;
	bool compressionEnabled = false;
	std::uint16_t compressionType = 0;
	bool encryptionEnabled = false;
	std::uint16_t encryptionType = 0;

	std::filesystem::path backupSource;
	std::filesystem::path backupDestination;
	std::filesystem::path restoreSource;
	std::filesystem::path restoreDestination;
};