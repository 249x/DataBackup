#pragma once

#include "../Manager.h"

#include <filesystem>

class FilePipelineManager : public Manager {
public:
	FilePipelineManager(System& sys);
	~FilePipelineManager() override;

	void Initialize() override;

    bool Backup(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath);
    bool Restore(const std::filesystem::path& srcPath, const std::filesystem::path& tarPath);
	

};