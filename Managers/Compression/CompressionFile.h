// #pragma once

// #include <cstddef>
// #include <cstdint>
// #include <vector>
// #include "../../FileStruct/StructuredFileContent.h"

// /*
// 文件结构
// uint16 magic; // 文件魔数
// uint16 version; // 版本号 
// uint32 size; // 文件大小
// uint16 type; // 压缩算法标识*
// uint8*size data; // 数据
// */

// class CompressionFile : public StructuredFileContent {
// public:
// 	CompressionFile();
// 	~CompressionFile() override;

// 	std::uint32_t GetMagic() override;
// 	std::uint32_t GetHeaderSizeBytes() override;

//     std::uint16_t GetType(){
//         return type;
//     }
//     void SetType(std::uint16_t type){
//         this->type = type;
//     }

// private:
// 	std::uint16_t type = 0;
	
// protected:
// 	bool SerializeHeader(std::vector<std::uint8_t>& data, std::size_t& offset) override;
// 	bool DeserializeHeader(const std::vector<std::uint8_t>& data, std::size_t& offset) override;

// };