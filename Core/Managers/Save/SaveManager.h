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

    bool AddSaveData(ISaveData* saveData);
    bool RemoveSaveData(std::uint32_t saveID);

private:
	// 只存个文件名；真正的路径挂在可执行文件旁边，见 SaveFilePath()
	static constexpr const char* SavePath = "save.dat";
    static constexpr const char* LogInfo = "Save";

    std::unordered_map<std::uint32_t, ISaveData*> saveManagers;

    // <可执行文件所在目录>/save.dat：不受“从哪个目录启动”影响
    std::filesystem::path SaveFilePath() const;

    void CollectSaveData();
};