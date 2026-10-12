#include "SaveManager.h"
#include "../FileIO/FileIOManager.h"
#include "../../General/PathUtils.h"
#include "../../General/SerializationUtils.h"
#include "../../General/Utf8.h"
#include "../../General/Debug.h"

SaveManager::SaveManager(System& sys) : Manager(sys) {}

// 存档放在可执行文件旁边：免得“从哪个目录启动”变成行为的一部分
std::filesystem::path SaveManager::SaveFilePath() const{
    return PathUtils::BesideExecutable(SavePath);
}

void SaveManager::Initialize(){
    CollectSaveData();
}


void SaveManager::OnDestroy(){
}

bool SaveManager::Save(){
    FileIOManager* IO = Get<FileIOManager>();
    if (!IO){
        Debug::Error("No file IO manager found", LogInfo);
        return false;
    }

    std::vector<std::uint8_t> saveData;
    std::size_t offset = 0;

    const std::filesystem::path path = SaveFilePath();

    for (auto& [ID, saveDataPtr] : saveManagers){
        std::vector<std::uint8_t> data;
        if(!saveDataPtr->Save(data)){
            Debug::Warning("Failed to save manager with ID: " + std::to_string(ID), LogInfo);
            continue;
        }
        if(!SerializationUtils::WriteUnsigned(saveData, offset, ID) ||
           !SerializationUtils::WriteBytes(saveData, offset, data)){
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

    std::size_t offset = 0;
    std::uint32_t ID;
    std::vector<std::uint8_t> data;

    while(SerializationUtils::ReadUnsigned(saveData, offset, ID)){
        if(!SerializationUtils::ReadBytes(saveData, offset, data)){
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