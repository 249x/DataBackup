#pragma once

#include "../../../FileStruct/FileMetaData.h"

#include <filesystem>

class FileMetaDataIO {
public:
	FileMetaDataIO() = default;
	FileMetaDataIO(const FileMetaDataIO&) = default;
	FileMetaDataIO& operator=(const FileMetaDataIO&) = default;
	virtual ~FileMetaDataIO();

	virtual bool Read(const std::filesystem::path& path,
	                  FileMetaData& metadata) const = 0;
	virtual bool Write(const std::filesystem::path& path,
	                   const FileMetaData& metadata) const = 0;
};
