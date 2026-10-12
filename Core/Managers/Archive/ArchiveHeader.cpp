// #include "ArchiveHeader.h"

// #include "../../General/SerializationUtils.h"

// ArchiveFile::ArchiveFile() : StructuredFileContent() {
// }

// ArchiveFile::~ArchiveFile() {
// }

// std::uint32_t ArchiveFile::GetMagic() {
// 	return 1;
// }

// std::uint32_t ArchiveFile::GetHeaderSizeBytes() {
// 	return StructuredFileContent::GetHeaderSizeBytes() + sizeof(std::uint32_t);
// }


// std::uint32_t ArchiveFile::EntryCount() const noexcept {
// 	return entryCount;
// }

// void ArchiveFile::SetEntryCount(std::uint32_t value) noexcept {
// 	entryCount = value;
// }

// bool ArchiveFile::SerializeHeader(std::vector<std::uint8_t>& data, std::size_t& offset) {
// 	if (!StructuredFileContent::SerializeHeader(data, offset) ||
// 		!SerializationUtils::WriteUnsigned(data, offset, entryCount)) {
// 		return false;
// 	}
// 	return true;
// }

// bool ArchiveFile::DeserializeHeader(const std::vector<std::uint8_t>& data, std::size_t& offset) {
// 	if (!StructuredFileContent::DeserializeHeader(data, offset) ||
// 		!SerializationUtils::ReadUnsigned(data, offset, entryCount)) {
// 		return false;
// 	}
// 	return true;
// }