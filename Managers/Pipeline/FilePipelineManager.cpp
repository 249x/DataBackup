#include "FilePipelineManager.h"

#include "../FileIO/FileIOManager.h"
#include "../Archive/ArchiveManager.h"
#include "../Command/CommandManager.h"
#include "../Compression/CompressionManager.h"
#include "../Encryption/EncryptionManager.h"

#include "../../General/Debug.h"

#include <iostream>

FilePipelineManager::FilePipelineManager(System& sys) : Manager(sys){

}

FilePipelineManager::~FilePipelineManager(){

}

void FilePipelineManager::Initialize(){
    CommandManager* command = Get<CommandManager>();

    command->RegisterCommand("backup", "Backup file tree by current settings", Backup, this);
    command->RegisterCommand("pack", "Pack file tree", Archive, this);
    command->RegisterCommand("compress", "Compress file tree", Compress, this);
    command->RegisterCommand("restore", "Restore file tree", Restore, this);
    command->RegisterCommand("encrypt", "Encrypt file tree", Encrypt, this);
    command->RegisterCommand("key", "Set path of key", SetKeyPath, this);

    // 设置 backup 要跑哪几个步骤
    command->RegisterCommand("set-archive", "Enable/disable packing in backup", SetArchive, this);
    command->RegisterCommand("set-compress", "Set compression: <enable> <algorithm>", SetCompression, this);
    command->RegisterCommand("set-encrypt", "Set encryption: <enable> <algorithm>", SetEncryption, this);
    command->RegisterCommand("settings", "Print backup settings", PrintSettings, this);
}

bool FilePipelineManager::Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    FileIOManager* IO = Get<FileIOManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }

    if (settings.encrypt && key.empty()) {
        Debug::Error("Encryption is enabled but no key is loaded, run 'key <path>' first",
                     "Pipeline");
        return false;
    }

    // 按设置依次叠加：打包 -> 压缩 -> 加密，每步都在上一步的产物外再包一层
    if (settings.archive && !PackEntries(entries)) {
        return false;
    }
    if (settings.compress && !CompressEntries(entries, settings.compressType)) {
        return false;
    }
    if (settings.encrypt && !EncryptEntries(entries, settings.encryptType)) {
        return false;
    }

    // 打包后的条目是派生数据，其元数据是空默认值，只能写内容；
    // 未打包时 entries 仍是源文件，单条目写文件、多条目写树
    if (settings.archive) {
        return IO->WriteContent(tarPath, entries.front().Content());
    }
    return WriteResult(tarPath, entries);
}

bool FilePipelineManager::Archive(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    FileIOManager* IO = Get<FileIOManager>();
    ArchiveManager* archive = Get<ArchiveManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }

    StructuredFileContent content;
    if (!archive->Pack(entries, content.CustomRef(), content.Data())) {
        Debug::Warning("Failed to pack: " + srcPath.string(), "Pipeline");
        return false;
    }
    content.SetOperation(1);

    std::vector<uint8_t> data;
    if (!content.Serialize(data)) {
        return false;
    }
    return IO->WriteContent(tarPath, data);
}

bool FilePipelineManager::Encrypt(const std::filesystem::path& srcPath,
                                  const std::filesystem::path& tarPath,
                                  const ::uint16_t& type) {
    FileIOManager* IO = Get<FileIOManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }
    if (!EncryptEntries(entries, type)) {
        return false;
    }
    return WriteResult(tarPath, entries);
}

bool FilePipelineManager::Compress(const std::filesystem::path& srcPath,
                                    const std::filesystem::path& tarPath,
                                    const ::uint16_t& type){
    FileIOManager* IO = Get<FileIOManager>();

    std::vector<FileEntry> entries;
    if (!IO->Read(srcPath, entries)) {
        return false;
    }
    if (!CompressEntries(entries, type)) {
        return false;
    }
    return WriteResult(tarPath, entries);
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

bool FilePipelineManager::SetArchive(bool enabled) {
    settings.archive = enabled;
    return true;
}

bool FilePipelineManager::SetCompression(bool enabled, std::uint16_t type) {
    if (enabled && Get<CompressionManager>()->Get(type) == nullptr) {
        Debug::Error("No compression handler with id " + std::to_string(type), "Pipeline");
        return false;
    }
    settings.compress = enabled;
    settings.compressType = type;
    return true;
}

bool FilePipelineManager::SetEncryption(bool enabled, std::uint16_t type) {
    settings.encrypt = enabled;
    settings.encryptType = type;
    return true;
}

bool FilePipelineManager::PrintSettings() {
    std::cout << "archive  : " << (settings.archive ? "on" : "off") << "\n"
              << "compress : " << (settings.compress ? "on" : "off")
              << " (algorithm " << settings.compressType << ")\n"
              << "encrypt  : " << (settings.encrypt ? "on" : "off")
              << " (algorithm " << settings.encryptType << ", key "
              << (key.empty() ? "missing" : "loaded") << ")\n";
    return true;
}

const BackupSettings& FilePipelineManager::Settings() const noexcept {
    return settings;
}

bool FilePipelineManager::WriteResult(const std::filesystem::path& tarPath,
                                      const std::vector<FileEntry>& entries) {
    FileIOManager* IO = Get<FileIOManager>();
    if (entries.size() == 1) {
        return IO->Write(tarPath, entries.front());
    }
    return IO->Write(tarPath, entries);
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
            // 不能跳过：留着未压缩的条目会让调用方以为压缩已生效
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
        if (!encryption->Encryption(entry.Content(), type, key, output)) {
            // 尤其不能跳过：未加密的数据混进"加密后"的归档是安全问题
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
    if (!encryption->Decryption(content.Data(), content.CustomRef(), key, entry.Content())) {
        return false;
    }
    return true;
}