#pragma once

#include "FileMetaDataIO.h"

class WindowsFileMetaDataIO final : public FileMetaDataIO {
public:
	WindowsFileMetaDataIO() = default;
	~WindowsFileMetaDataIO() override = default;

	bool Read(const std::filesystem::path& path, FileMetaData& metadata) const override;
	bool Write(const std::filesystem::path& path, const FileMetaData& metadata) const override;
};
