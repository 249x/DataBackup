#include "FilePipelineManager.h"

#include "../FileIO/FileIOManager.h"
#include "../Archive/ArchiveManager.h"
#include "../Command/CommandManager.h"
#include "../Compression/CompressionManager.h"
#include "../Encryption/EncryptionManager.h"

#include "../../General/ErrorHandler.h"

FilePipelineManager::FilePipelineManager(System& sys) : Manager(sys){

}

FilePipelineManager::~FilePipelineManager(){

}

void FilePipelineManager::Initialize(){
    CommandManager* command = Get<CommandManager>();

    command->RegisterCommand("backup", "Backup file tree", Backup, this);
    command->RegisterCommand("pack", "Pack file tree", Archive, this);
    command->RegisterCommand("restore", "Restore file tree", Restore, this);
    command->RegisterCommand("encrypt", "Encrypt file tree", Encrypt, this);
}

bool FilePipelineManager::Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    FileIOManager* IO = Get<FileIOManager>();
    std::vector<FileEntry> entries;
    IO->Read(srcPath, entries);
    IO->Write(tarPath, entries);
    return true;
}

bool FilePipelineManager::Archive(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    FileIOManager* IO = Get<FileIOManager>();
    ArchiveManager* archive = Get<ArchiveManager>();

    std::vector<FileEntry> entries;
    IO->Read(srcPath, entries);

    StructuredFileContent content;
    archive->Pack(entries, content.CustomRef(), content.Data());
    content.SetOperation(1);

    FileEntry entry;
    content.Serialize(entry.Content());
    IO->Write(tarPath, entry);

    return true;
}

bool FilePipelineManager::Encrypt(const std::filesystem::path& srcPath,
                                  const std::filesystem::path& tarPath,
                                  const std::filesystem::path& keyPath,
                                  const ::uint16_t& type) {
    FileIOManager* IO = Get<FileIOManager>();
    EncryptionManager* encryption = Get<EncryptionManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }

    FileEntry key;
    if (!IO->Read(keyPath, key)) {
        return false;
    }

    for (FileEntry& entry : entries) {
        std::vector<std::uint8_t> output;
        if (!encryption->Decryption(entry.Content(), type, key.Content(), output)) {
            continue;
        }
        StructuredFileContent content(output, 3, type);
        if (!content.Serialize(entry.Content())) {
            return false;
        }
    }
    return IO->Write(tarPath, entries);
}

bool FilePipelineManager::Restore(const std::filesystem::path& srcPath,
                                  const std::filesystem::path& tarPath,
                                    const std::filesystem::path& keyPath) {
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

    std::vector<FileEntry> plainFiles;

    while (!entryStack.empty()) {
        FileEntry current = std::move(entryStack.top());
        entryStack.pop();

        StructuredFileContent content;
        if (!content.Deserialize(current.Content())) {
            plainFiles.push_back(std::move(current));
            continue;
        }

        FileEntry key;
        IO->Read(keyPath, key);

        switch (content.Operation()) {
            case 1:
                DealArchive(content, entryStack);
                break;
            case 3:
                DealEncrypt(content, entryStack, key.Content());
                break;
            default:
                plainFiles.push_back(std::move(current));
                break;
        }
    }

    return IO->Write(tarPath, plainFiles);
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

bool FilePipelineManager::DealEncrypt(const StructuredFileContent& content, 
    std::stack<FileEntry>& stack, 
    const std::vector<uint8_t>& key){
    EncryptionManager* encryption = Get<EncryptionManager>();

    std::vector<uint8_t> plain;
    if (!encryption->Decryption(content.Data(), content.CustomRef(), key, plain)) {
        return false;
    }

    FileEntry entry;
    entry.Content() = std::move(plain);
    stack.push(std::move(entry));
    return true;
}