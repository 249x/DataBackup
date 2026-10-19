#include "System.h"
#include "Managers/Manager.h"
#include "Managers/FileIO/FileIOManager.h"
#include "Managers/Archive/ArchiveManager.h"
#include "Managers/Encryption/EncryptionManager.h"
#include "Managers/TestCase/TestManager.h"
#include "Managers/Compression/CompressionManager.h"
#include "Managers/Filter/FilterManager.h"
#include "Managers/Pipeline/FilePipelineManager.h"
#include "Managers/Command/CommandManager.h"
#include "Managers/Save/SaveManager.h"

System::System()
{
    Register<FileIOManager>();
    Register<ArchiveManager>();
    Register<EncryptionManager>();
    Register<CompressionManager>();
    Register<FilterManager>();
    Register<FilePipelineManager>();
    Register<CommandManager>();
    Register<TestManager>();
    Register<SaveManager>();
    
    managers.ForEach([](Manager* obj) {
        obj->Initialize();
    });

    
}

System::~System(){
    managers.ForEach([](Manager* obj) {
        obj->OnDestroy();
    });
}

void System::Start(){
    //Get<TestManager>()->RunAll();

    FileIOManager* IO = Get<FileIOManager>();
    CommandManager* command = Get<CommandManager>();

    while (true){
        std::filesystem::path path = IO->CurrentPath();
        if (!command->RunConsole(path.string() + "|> ")) {
            break;
        }
    }
}

int System::Run(int argc, char* argv[]) {
    CommandManager* command = Get<CommandManager>();
    if (command != nullptr) {
        const std::vector<std::string> args = CommandManager::CommandLineArguments(argc, argv);
        if (args.size() > 1) {
            return command->Execute("run \"" + args[1] + "\"") ? 0 : 1;
        }
    }

    Start();
    return 0;
}