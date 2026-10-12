// #pragma once

// #include <cstdint>
// #include <vector>
// #include "../../FileStruct/StructuredFileContent.h"

// /*
// 打包文件结构
// uint32 magic; // 文件魔数
// uint32 version; // 版本号
// uint64 size; // 数据大小
// uint32 count; // 文件数量*
// uint8*size data; // 打包文件完整数据
// 单个文件的大小均由序列化工具自带写入每个文件开头
// */

// class ArchiveFile : public StructuredFileContent {
// public:
// 	ArchiveFile();
// 	~ArchiveFile() override;

// 	std::uint32_t GetMagic() override;
// 	std::uint32_t GetHeaderSizeBytes() override;

// 	std::uint32_t EntryCount() const noexcept;
// 	void SetEntryCount(std::uint32_t value) noexcept;

// private:
// 	std::uint32_t entryCount = 0;
	
// protected:
// 	bool SerializeHeader(std::vector<std::uint8_t>& data, std::size_t& offset) override;
// 	bool DeserializeHeader(const std::vector<std::uint8_t>& data, std::size_t& offset) override;
	
// };