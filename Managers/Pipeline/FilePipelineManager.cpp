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
}

bool FilePipelineManager::Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    FileIOManager* IO = Get<FileIOManager>();
    ArchiveManager* archive = Get<ArchiveManager>();
    CompressionManager* compression = Get<CompressionManager>();
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

    if (encryption->Enabled() && !encryption->CheckKey()) {
        return false;
    }

    if (archive->Enabled() && !PackEntries(entries)) {
        return false;
    }
    if (compression->Enabled() && !CompressEntries(entries, compression->Type())) {
        return false;
    }
    if (encryption->Enabled() && !EncryptEntries(entries, encryption->Type())) {
        return false;
    }

    if (archive->Enabled()) {
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
    ArchiveManager* archive = Get<ArchiveManager>();
    CompressionManager* compression = Get<CompressionManager>();
    EncryptionManager* encryption = Get<EncryptionManager>();

    std::cout << "archive  : " << (archive->Enabled() ? "on" : "off") << "\n"
              << "compress : " << (compression->Enabled() ? "on" : "off")
              << " (algorithm " << compression->Type() << ")\n"
              << "encrypt  : " << (encryption->Enabled() ? "on" : "off")
              << " (algorithm " << encryption->Type() << ", key "
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