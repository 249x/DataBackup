#include "FilePipelineManager.h"

#include "../FileIO/FileIOManager.h"
#include "../Archive/ArchiveManager.h"
#include "../Command/CommandManager.h"
#include "../Compression/CompressionManager.h"
#include "../Encryption/EncryptionManager.h"

#include "../../General/Debug.h"

FilePipelineManager::FilePipelineManager(System& sys) : Manager(sys){

}

FilePipelineManager::~FilePipelineManager(){

}

void FilePipelineManager::Initialize(){
    CommandManager* command = Get<CommandManager>();

    command->RegisterCommand("backup", "Backup file tree", Backup, this);
    command->RegisterCommand("pack", "Pack file tree", Archive, this);
    command->RegisterCommand("compress", "Compress file tree", Compress, this);
    command->RegisterCommand("restore", "Restore file tree", Restore, this);
    command->RegisterCommand("encrypt", "Encrypt file tree", Encrypt, this);
    command->RegisterCommand("key", "Set path of key", SetKeyPath, this);
}

bool FilePipelineManager::Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    FileIOManager* IO = Get<FileIOManager>();
    std::vector<FileEntry> entries;
    if(!IO->Read(srcPath, entries)||
        !IO->Write(tarPath, entries)){
        return false;
    }
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

    std::vector<uint8_t> data;
    content.Serialize(data);
    IO->WriteContent(tarPath, data);

    return true;
}

bool FilePipelineManager::Encrypt(const std::filesystem::path& srcPath,
                                  const std::filesystem::path& tarPath,
                                  const ::uint16_t& type) {
    FileIOManager* IO = Get<FileIOManager>();
    EncryptionManager* encryption = Get<EncryptionManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }

    for (FileEntry& entry : entries) {
        std::vector<std::uint8_t> output;
        if (!encryption->Encryption(entry.Content(), type, key, output)) {
            continue;
        }
        StructuredFileContent content(output, 3, type);
        if (!content.Serialize(entry.Content())) {
            return false;
        }
    }
    return IO->Write(tarPath, entries);
}

bool FilePipelineManager::Compress(const std::filesystem::path& srcPath,
                                    const std::filesystem::path& tarPath,
                                    const ::uint16_t& type){
    FileIOManager* IO = Get<FileIOManager>();
    CompressionManager* compression = Get<CompressionManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }

    for (FileEntry& entry : entries) {
        std::vector<std::uint8_t> output;
        if (!compression->Compression(entry.Content(), type, output)) {
            continue;
        }
        StructuredFileContent content(output, 2, type);
        if (!content.Serialize(entry.Content())) {
            return false;
        }
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

    std::vector<FileEntry> plainFiles;

    while (!entryStack.empty()) {
        FileEntry current = std::move(entryStack.top());
        entryStack.pop();

        StructuredFileContent content;
        if (!content.Deserialize(current.Content())) {
            plainFiles.push_back(std::move(current));
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
                plainFiles.push_back(std::move(current));
                break;
        }
    }

    return IO->Write(tarPath, plainFiles);
}

bool FilePipelineManager::SetKeyPath(const std::filesystem::path& keyPath){
    FileIOManager* IO = Get<FileIOManager>();
    if(!IO->ReadContent(keyPath, key)){
        return false;
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
    if (!encryption->Decryption(content.Data(), content.CustomRef(), key, entry.Content())) {
        return false;
    }
    return true;
}