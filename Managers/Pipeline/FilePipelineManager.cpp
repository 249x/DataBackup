#include "FilePipelineManager.h"

#include "../FileIO/FileIOManager.h"
#include "../Archive/ArchiveManager.h"
#include "../Command/CommandManager.h"
#include "../Compression/CompressionManager.h"
#include "../../FileStruct/StructuredFileContent.h"

FilePipelineManager::FilePipelineManager(System& sys) : Manager(sys){

}

FilePipelineManager::~FilePipelineManager(){

}

void FilePipelineManager::Initialize(){
    CommandManager* command = Get<CommandManager>();

    command->RegisterCommand("backup", "Backup file tree", Backup, this);
}

bool FilePipelineManager::Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    FileIOManager* IO = Get<FileIOManager>();
    std::vector<FileEntry> entries;
    IO->Read(srcPath, entries);
    IO->Write(tarPath, entries);
    return true;
}

bool FilePipelineManager::Restore(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath){
    return true;
}

