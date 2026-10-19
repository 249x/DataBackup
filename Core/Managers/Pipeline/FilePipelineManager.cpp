#include "FilePipelineManager.h"

#include "../FileIO/FileIOManager.h"
#include "../Archive/ArchiveManager.h"
#include "../Command/CommandManager.h"
#include "../Compression/CompressionManager.h"
#include "../Encryption/EncryptionManager.h"
#include "../Filter/FilterManager.h"

#include "../../General/Debug.h"
#include "../../General/SerializationUtils.h"
#include "../../General/Utf8.h"

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

    command->RegisterCommand("set-archive", "Enable/disable packing in backup",
                             SetArchive, this);
    command->RegisterCommand("set-compress", "Set compression: <enable> <algorithm>",
                             SetCompression, this);
    command->RegisterCommand("set-encrypt", "Set encryption: <enable> <algorithm>",
                             SetEncryption, this);
}

bool FilePipelineManager::SetArchive(bool enabled){
    archiveEnabled = enabled;
    return true;
}


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

bool FilePipelineManager::Save(std::vector<std::uint8_t>& data){
    data.clear();
    SerializationUtils io;

    const bool ok =
        io.WriteUnsigned(data, static_cast<std::uint8_t>(archiveEnabled ? 1 : 0)) &&
        io.WriteUnsigned(data, static_cast<std::uint8_t>(compressionEnabled ? 1 : 0)) &&
        io.WriteUnsigned(data, compressionType) &&
        io.WriteUnsigned(data, static_cast<std::uint8_t>(encryptionEnabled ? 1 : 0)) &&
        io.WriteUnsigned(data, encryptionType);
    if (!ok){
        Debug::Error("Failed to serialize pipeline settings", "Pipeline");
        return false;
    }
    return true;
}

bool FilePipelineManager::Load(const std::vector<std::uint8_t>& data){
    SerializationUtils io;
    std::uint8_t archive = 0;
    std::uint8_t compression = 0;
    std::uint8_t encryption = 0;
    std::uint16_t compressionId = 0;
    std::uint16_t encryptionId = 0;

    if (!io.ReadUnsigned(data, archive) ||
        !io.ReadUnsigned(data, compression) ||
        !io.ReadUnsigned(data, compressionId) ||
        !io.ReadUnsigned(data, encryption) ||
        !io.ReadUnsigned(data, encryptionId)){
        Debug::Error("Saved pipeline settings are too short", "Pipeline");
        return false;
    }

    SetArchive(archive != 0);
    const bool compressionOk = SetCompression(compression != 0, compressionId);
    const bool encryptionOk = SetEncryption(encryption != 0, encryptionId);
    return compressionOk && encryptionOk;
}

std::uint32_t FilePipelineManager::SaveID() const{
    return SaveIDValue;
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

std::string FilePipelineManager::Where(const FileEntry& entry) {
    const std::filesystem::path& path = entry.Path();
    return path.empty() ? std::string{} : ": " + Utf8::FromPath(path);
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

    bool degraded = false;

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
                if (!DealArchive(content, entryStack)) {
                    Debug::Warning("Failed to unpack archive" + Where(current) +
                                     ", kept as-is",
                                 "Pipeline");
                    degraded = true;
                    entries.push_back(std::move(current));
                    break;
                }
                break;
            case 2:
                if (!DealCompress(content, current)) {
                    Debug::Warning("Failed to decompress entry" + Where(current) +
                                     ", kept as-is (corrupted backup?)",
                                 "Pipeline");
                    degraded = true;
                    entries.push_back(std::move(current));
                    break;
                }
                entryStack.push(std::move(current));
                break;
            case 3:
                if (!DealEncrypt(content, current)) {
                    Debug::Warning("Failed to decrypt entry" + Where(current) +
                                     ", kept as-is (load the backup's key first: key <Path>)",
                                 "Pipeline");
                    degraded = true;
                    entries.push_back(std::move(current));
                    break;
                }
                entryStack.push(std::move(current));
                break;
            default:
                Debug::Warning("Unkown operation type", "Pipeline");
                entries.push_back(std::move(current));
                break;
        }
    }

    const bool written = IO->Write(tarPath, entries);
    return written && !degraded;
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