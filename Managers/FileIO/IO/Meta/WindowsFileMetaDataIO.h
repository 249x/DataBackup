#pragma once

#include "FileMetaDataIO.h"

#include <memory>

class WindowsMetaDataUtils;

class WindowsFileMetaDataIO final : public FileMetaDataIO {
public:
	WindowsFileMetaDataIO();
	~WindowsFileMetaDataIO() override;

	bool Read(const std::filesystem::path& path, FileMetaData& metadata) const override;
	bool Write(const std::filesystem::path& path, const FileMetaData& metadata) const override;

private:
	// 只在 Windows 上存在：该工具类的接口依赖 <windows.h>，不能出现在本头文件里
#ifdef _WIN32
	std::unique_ptr<WindowsMetaDataUtils> utils;
#endif
};
