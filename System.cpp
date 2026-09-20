#include "System.h"
#include "Managers/Manager.h"
#include "Managers/FileIO/FileIOManager.h"
#include "Managers/Archive/ArchiveManager.h"
#include "Managers/Encryption/EncryptionManager.h"
#include "Managers/TestCase/TestManager.h"
#include "Managers/Compression/CompressionManager.h"
#include "Managers/Pipeline/FilePipelineManager.h"
#include "Managers/Command/CommandManager.h"

System::System()
{
    Register<FileIOManager>();
    Register<ArchiveManager>();
    Register<EncryptionManager>();
    Register<CompressionManager>();
    Register<FilePipelineManager>();
    Register<CommandManager>();
    Register<TestManager>();
    
    managers.ForEach([](Manager* obj) {
        obj->Initialize();
    });

    
}

System::~System(){
    
}

void System::Start(){
    //Get<TestManager>()->RunAll();

    FileIOManager* IO = Get<FileIOManager>();
    CommandManager* command = Get<CommandManager>();

    while (true){
        std::filesystem::path path = IO->CurrentPath();
        command->RunConsole(path.string() + "|> ");
    }
}