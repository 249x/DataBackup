#include "FilePipelineManager.h"

#include "../FileIO/FileIOManager.h"
#include "../Archive/ArchiveManager.h"
#include "../Command/CommandManager.h"
#include "../Compression/CompressionManager.h"
#include "../Encryption/EncryptionManager.h"
#include "../Filter/FilterManager.h"

#include "../../General/Debug.h"

#include <algorithm>
#include <iostream>

FilePipelineManager::FilePipelineManager(System& sys) : Manager(sys){

}

FilePipelineManager::~FilePipelineManager(){

}

void FilePipelineManager::Initialize(){
    CommandManager* command = Get<CommandManager>();

    command->RegisterCommand("backup", "Backup file tree by current settings", Backup, this);
    command->RegisterCommand("restore", "Restore file tree", Restore, this);

    command->RegisterCommand("settings", "Print backup settings", PrintSettings, this);

    // 三个开关是整条流水线的设置，所以也由流水线注册命令（名字与原来保持一致）
    command->RegisterCommand("set-archive", "Enable/disable packing in backup",
                             SetArchive, this);
    command->RegisterCommand("set-compress", "Set compression: <enable> <algorithm>",
                             SetCompression, this);
    command->RegisterCommand("set-encrypt", "Set encryption: <enable> <algorithm>",
                             SetEncryption, this);
}

// 打包没有可失败的地方：关掉时 Backup 里那一步整段跳过
bool FilePipelineManager::SetArchive(bool enabled){
    archiveEnabled = enabled;
    return true;
}

// 压缩/加密要校验算法号：开着却指了个不存在的算法，那是设置错误
bool FilePipelineManager::SetCompression(bool enabled, std::uint16_t type){
    CompressionManager* compression = Get<CompressionManager>();
    if (compression == nullptr || (enabled && compression->Get(type) == nullptr)){
        Debug::Error("No compression handler with id " + std::to_string(type), "Pipeline");
        return false;
    }
    compressionEnabled = enabled;
    compressionType = type;
    return true;
}

bool FilePipelineManager::SetEncryption(bool enabled, std::uint16_t type){
    EncryptionManager* encryption = Get<EncryptionManager>();
    if (encryption == nullptr || (enabled && encryption->Get(type) == nullptr)){
        Debug::Error("No encryption handler with id " + std::to_string(type), "Pipeline");
        return false;
    }
    encryptionEnabled = enabled;
    encryptionType = type;
    return true;
}

bool FilePipelineManager::ArchiveEnabled() const noexcept{
    return archiveEnabled;
}

bool FilePipelineManager::CompressionEnabled() const noexcept{
    return compressionEnabled;
}

bool FilePipelineManager::EncryptionEnabled() const noexcept{
    return encryptionEnabled;
}

std::uint16_t FilePipelineManager::CompressionType() const noexcept{
    return compressionType;
}

std::uint16_t FilePipelineManager::EncryptionType() const noexcept{
    return encryptionType;
}

bool FilePipelineManager::Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    FileIOManager* IO = Get<FileIOManager>();
    EncryptionManager* encryption = Get<EncryptionManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }

    FilterEntries(entries);
    if (entries.empty()) {
        Debug::Warning("All entries are filtered out, nothing to back up", "Pipeline");
        return false;
    }

    if (encryptionEnabled && !encryption->CheckKey(encryptionType)) {
        return false;
    }

    if (archiveEnabled && !PackEntries(entries)) {
        return false;
    }
    if (compressionEnabled && !CompressEntries(entries, compressionType)) {
        return false;
    }
    if (encryptionEnabled && !EncryptEntries(entries, encryptionType)) {
        return false;
    }

    if (archiveEnabled) {
        return IO->WriteContent(tarPath, entries.front().Content());
    }
    if (entries.size() == 1) {
        return IO->Write(tarPath, entries.front());
    }
    return IO->Write(tarPath, entries);
}

bool FilePipelineManager::Restore(const std::filesystem::path& srcPath,
                                  const std::filesystem::path& tarPath) {
    FileIOManager* IO = Get<FileIOManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }

    std::stack<FileEntry> entryStack;
    for (FileEntry& entry : entries) {
        entryStack.push(std::move(entry));
    }
    entries.clear();

    while (!entryStack.empty()) {
        FileEntry current = std::move(entryStack.top());
        entryStack.pop();

        StructuredFileContent content;
        if (!content.Deserialize(current.Content())) {
            entries.push_back(std::move(current));
            continue;
        }
        switch (content.Operation()) {
            case 1:
                DealArchive(content, entryStack);
                break;
            case 2:
                DealCompress(content, current);
                entryStack.push(current);
                break;
            case 3:
                DealEncrypt(content, current);
                entryStack.push(current);
                break;
            default:
                Debug::Warning("Unkown operation type", "Pipeline");
                entries.push_back(std::move(current));
                break;
        }
    }
    return IO->Write(tarPath, entries);
}

bool FilePipelineManager::PrintSettings() {
    EncryptionManager* encryption = Get<EncryptionManager>();

    std::cout << "archive  : " << (archiveEnabled ? "on" : "off") << "\n"
              << "compress : " << (compressionEnabled ? "on" : "off")
              << " (algorithm " << compressionType << ")\n"
              << "encrypt  : " << (encryptionEnabled ? "on" : "off")
              << " (algorithm " << encryptionType << ", key "
              << (encryption->HasKey() ? "loaded" : "missing") << ")\n";
    return true;
}

void FilePipelineManager::FilterEntries(std::vector<FileEntry>& entries) {
    const FilterManager* filter = Get<FilterManager>();
    if (filter == nullptr) {
        return;
    }

    const std::size_t before = entries.size();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [filter](const FileEntry& entry) {
                                     return !filter->Include(entry.MetaData());
                                 }),
                  entries.end());

    const std::size_t removed = before - entries.size();
    if (removed > 0) {
        Debug::Info("Filtered out " + std::to_string(removed) + " of " +
                        std::to_string(before) + " entries",
                    "Pipeline");
    }
}

bool FilePipelineManager::PackEntries(std::vector<FileEntry>& entries) {
    ArchiveManager* archive = Get<ArchiveManager>();

    StructuredFileContent content;
    if (!archive->Pack(entries, content.CustomRef(), content.Data())) {
        Debug::Error("Failed to pack entries", "Pipeline");
        return false;
    }
    content.SetOperation(1);

    FileEntry packed;
    if (!content.Serialize(packed.Content())) {
        return false;
    }
    entries.clear();
    entries.emplace_back(std::move(packed));
    return true;
}

bool FilePipelineManager::CompressEntries(std::vector<FileEntry>& entries,
                                          std::uint16_t type) {
    CompressionManager* compression = Get<CompressionManager>();

    for (FileEntry& entry : entries) {
        std::vector<std::uint8_t> output;
        if (!compression->Compression(entry.Content(), type, output)) {
            Debug::Error("Failed to compress entry: " + entry.Path().string(), "Pipeline");
            return false;
        }
        StructuredFileContent content(output, 2, type);
        if (!content.Serialize(entry.Content())) {
            return false;
        }
    }
    return true;
}

bool FilePipelineManager::EncryptEntries(std::vector<FileEntry>& entries,
                                         std::uint16_t type) {
    EncryptionManager* encryption = Get<EncryptionManager>();

    for (FileEntry& entry : entries) {
        std::vector<std::uint8_t> output;
        if (!encryption->Encryption(entry.Content(), type, output)) {
            Debug::Error("Failed to encrypt entry: " + entry.Path().string(), "Pipeline");
            return false;
        }
        StructuredFileContent content(output, 3, type);
        if (!content.Serialize(entry.Content())) {
            return false;
        }
    }
    return true;
}

bool FilePipelineManager::DealArchive(const StructuredFileContent& content,
                                      std::stack<FileEntry>& stack) {
    ArchiveManager* archive = Get<ArchiveManager>();
    std::vector<FileEntry> entries;
    if (!archive->Unpack(content.Data(), content.CustomRef(), entries)) {
        return false;
    }
    for (FileEntry& entry : entries) {
        stack.push(std::move(entry));
    }
    return true;
}

bool FilePipelineManager::DealCompress(const StructuredFileContent& content, 
    FileEntry& entry){
    CompressionManager* compression = Get<CompressionManager>();
    if (!compression->Decompression(content.Data(), content.CustomRef(), entry.Content())) {
        return false;
    }
    return true;

}

bool FilePipelineManager::DealEncrypt(const StructuredFileContent& content, 
    FileEntry& entry){
    EncryptionManager* encryption = Get<EncryptionManager>();
    if (!encryption->Decryption(content.Data(), content.CustomRef(), entry.Content())) {
        return false;
    }
    return true;
}