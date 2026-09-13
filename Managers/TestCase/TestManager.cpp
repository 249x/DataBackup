#include "TestManager.h"
#include "../FileIO/FileIOManager.h"
#include "../Archive/ArchiveManager.h"
#include "../Command/CommandManager.h"
#include "../Compression/CompressionManager.h"
#include "../../FileStruct/StructuredFileContent.h"

#include<vector>

TestManager::TestManager(System& sys) : Manager(sys)
{
}

TestManager::~TestManager()
{
}

void TestManager::Initialize()
{
    Register("Simple Pack Test", [this]() 
    {
        FileIOManager* fileIOManager = Get<FileIOManager>();
        ArchiveManager* fileArchiveManager = Get<ArchiveManager>();

        if (!fileIOManager || !fileArchiveManager) {
            return false;
        }

        std::vector<FileEntry> entries;

        // file -> entries
        if (!fileIOManager->Read("Test/A", entries)) {
            return false;
        }

        // entries -> archive
        std::vector<uint8_t> archive;
        std::uint16_t count;
        if (!fileArchiveManager->Pack(entries, count, archive)) {
            return false;
        }

        StructuredFileContent content(archive, 1, count);

        std::vector<uint8_t> output;
        content.Serialize(output);
        FileEntry archiveEntry(output, FileMetaData());

        // entry -> file
        if (!fileIOManager->Write("Test/B.dtpc", archiveEntry)) {
            return false;
        }

        return true;
    });

    Register("Simple Unpack Test", [this]() 
    {
        FileIOManager* fileIOManager = Get<FileIOManager>();
        ArchiveManager* fileArchiveManager = Get<ArchiveManager>();

        if (!fileIOManager || !fileArchiveManager) {
            return false;
        }

        // file -> entry
        FileEntry entry;
        if (!fileIOManager->Read("Test/B.dtpc", entry)) {
            return false;
        }
        
        StructuredFileContent content;
        content.Deserialize(entry.Content());

        // content -> entries
        std::vector<FileEntry> entries;
        if (!fileArchiveManager->Unpack(content.Data(), content.Sign(), entries)) {
            return false;
        }

        // entries -> file
        if(!fileIOManager->Write("Test/C", entries)) {
            return false;
        }

        return true;
    });

    Register("Simple Compression Test", [this]() 
    {
        FileIOManager* fileIOManager = Get<FileIOManager>();
        CompressionManager* compressionManager = Get<CompressionManager>();

        // file -> entry
        FileEntry entry;
        if (!fileIOManager->Read("Test/B.dtpc", entry)) {
            return false;
        }
        
        std::vector<uint8_t> result;
        compressionManager->Compression(entry.Content(), 1, result);
        
        StructuredFileContent content(result, 2, 1);
        content.Serialize(entry.Content());

        // entry -> file
        if(!fileIOManager->Write("Test/D/1.txt", entry)) {
            return false;
        }

        return true;
    });
}


void TestManager::Register(const std::string& name, std::function<bool()> fn, bool expected) {
    cases.emplace_back(name, fn, expected);
}

void TestManager::RunAll() const {
    for (const auto& tc : cases) {
        bool result = tc.run();
        std::cout << (result == tc.expected ? "[PASS]" : "[FAIL]")
                  << " " << tc.name << std::endl;
    }
}