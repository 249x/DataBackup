#pragma once

#include "../Manager.h"
#include "ISaveData.h"

#include <cstdint>
#include <filesystem>
#include <unordered_map>

class SaveManager : public Manager
{
public:
    explicit SaveManager(System& sys);

    void Initialize() override;
    void OnDestroy() override;

    bool Save();
    bool Load();
    bool IsSaveFileExists();

    bool AddSaveData(ISaveData* saveData);
    bool RemoveSaveData(std::uint32_t saveID);

private:
	static constexpr const char* SavePath = "save.dat";
    static constexpr const char* LogInfo = "Save";

    std::unordered_map<std::uint32_t, ISaveData*> saveManagers;
    std::filesystem::path SaveFilePath();

    void CollectSaveData();
};