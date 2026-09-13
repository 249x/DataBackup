// #include "CompressionFile.h"
// #include "../../General/SerializationUtils.h"


// CompressionFile::CompressionFile(){

// }

// CompressionFile::~CompressionFile() {
// }


// std::uint32_t CompressionFile::GetMagic(){
//     return std::uint32_t(2);
// }

// std::uint32_t CompressionFile::GetHeaderSizeBytes(){
//     return sizeof(uint16_t) * 3 + sizeof(uint32_t) * 1;
// }

// bool CompressionFile::SerializeHeader(std::vector<std::uint8_t>& data, std::size_t& offset){
//     if(!StructuredFileContent::SerializeHeader(data, offset)||
//         !SerializationUtils::WriteUnsigned(data, offset ,type)){
//             return false;
//     }
//     return true;
// }


// bool CompressionFile::DeserializeHeader(const std::vector<std::uint8_t>& data, std::size_t& offset){
//     StructuredFileContent::DeserializeHeader(data, offset);
//     SerializationUtils::ReadUnsigned(data, offset, type);
//     return true;
// }