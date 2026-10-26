#include "SaveManager.h"
#include "../Command/CommandManager.h"
#include "../FileIO/FileIOManager.h"
#include "../../General/SerializationUtils.h"
#include "../../General/Utf8.h"
#include "../../General/Debug.h"

SaveManager::SaveManager(System& sys) : Manager(sys) {}

std::filesystem::path SaveManager::SaveFilePath(){
    FileIOManager* IO = Get<FileIOManager>();
    return IO != nullptr ? IO->BesideExecutable(SavePath) : std::filesystem::path(SavePath);
}

void SaveManager::Initialize(){
    CollectSaveData();

    CommandManager* command = Get<CommandManager>();
    if (command == nullptr) {
        Debug::Warning("No command manager found, save / load are unavailable", LogInfo);
        return;
    }

    command->RegisterCommand("save", "Save settings and filters to save.dat", Save, this);
    command->RegisterCommand("load", "Load settings and filters from save.dat", Load, this);
    command->RegisterCommand("delete", "Delete save.dat (settings and filters untouched)",
                             Delete, this);
}


bool SaveManager::IsSaveFileExists(){
    FileIOManager* IO = Get<FileIOManager>();
    if (!IO){
        Debug::Error("No file IO manager found", LogInfo);
        return false;
    }
    return IO->Exists(SaveFilePath());
}

bool SaveManager::Delete(){
    FileIOManager* IO = Get<FileIOManager>();
    if (!IO){
        Debug::Error("No file IO manager found", LogInfo);
        return false;
    }

    const std::filesystem::path path = SaveFilePath();
    if (!IO->Exists(path)){
        Debug::Warning("Nothing to delete: " + Utf8::FromPath(path), LogInfo);
        return false;
    }

    // 只删文件：设置与筛选条件还活在内核里，调用方想"恢复出厂"得自己去改
    if (!IO->Delete(path)){
        Debug::Error("Failed to delete save data: " + Utf8::FromPath(path), LogInfo);
        return false;
    }
    return true;
}

bool SaveManager::Save(){
    FileIOManager* IO = Get<FileIOManager>();
    if (!IO){
        Debug::Error("No file IO manager found", LogInfo);
        return false;
    }

    std::vector<std::uint8_t> saveData;
    SerializationUtils io;

    const std::filesystem::path path = SaveFilePath();

    for (auto& [ID, saveDataPtr] : saveManagers){
        std::vector<std::uint8_t> data;
        if(!saveDataPtr->Save(data)){
            Debug::Warning("Failed to save manager with ID: " + std::to_string(ID), LogInfo);
            continue;
        }
        if(!io.WriteUnsigned(saveData, ID) ||
           !io.WriteBytes(saveData, data)){
            Debug::Error("Failed to serialize save data", LogInfo);
            return false;
        }
    }

    if(!IO->WriteContent(path, saveData)){
        Debug::Error("Failed to write save data: " + Utf8::FromPath(path), LogInfo);
        return false;
    }
    return true;
}

bool SaveManager::Load(){
    FileIOManager* IO = Get<FileIOManager>();
    const std::filesystem::path path = SaveFilePath();

    std::vector<std::uint8_t> saveData;
    if(!IO->ReadContent(path, saveData)){
        Debug::Error("Failed to load save data: " + Utf8::FromPath(path), LogInfo);
        return false;
    }

    SerializationUtils io;
    std::uint32_t ID;
    std::vector<std::uint8_t> data;

    while(io.ReadUnsigned(saveData, ID)){
        if(!io.ReadBytes(saveData, data)){
            break;
        }
        auto it = saveManagers.find(ID);
        if (it == saveManagers.end()) {
            Debug::Warning("No save manager found for ID: " + std::to_string(ID), LogInfo);
            continue;
        }
        if (!it->second->Load(data)){
            Debug::Warning("Failed to load save manager with ID: " + std::to_string(ID), LogInfo);
            continue;
        }
    }
    return true;
}

bool SaveManager::AddSaveData(ISaveData* saveData){
    if (!saveData) {
        return false;
    }
    saveManagers[saveData->SaveID()] = saveData;
    return true;
}

bool SaveManager::RemoveSaveData(std::uint32_t saveID){
    auto it = saveManagers.find(saveID);
    if (it != saveManagers.end()) {
        saveManagers.erase(it);
        return true;
    }
    return false;
}


void SaveManager::CollectSaveData(){
    GetSystem().ForEach([this](Manager* obj) {
        if (auto* p = dynamic_cast<ISaveData*>(obj)) {
            saveManagers[p->SaveID()] = p;
        }
    });
}