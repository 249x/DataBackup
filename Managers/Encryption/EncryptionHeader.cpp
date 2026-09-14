#include "EncryptionHeader.h"

#include "../../General/SerializationUtils.h"

#include <limits>
#include <utility>

std::uint16_t EncryptionHeader::Algorithm() const noexcept {
        return algorithm;
}

void EncryptionHeader::SetAlgorithm(std::uint16_t value) noexcept {
        algorithm = value;
}

const std::vector<std::uint8_t>& EncryptionHeader::AlgorithmInfo() const noexcept {
        return algorithmInfo;
}

void EncryptionHeader::SetAlgorithmInfo(const std::vector<std::uint8_t>& value) {
        algorithmInfo = value;
}

std::size_t EncryptionHeader::HeaderSize() const noexcept {
        return headerSize;
}

EncryptionHeader::SerializedData EncryptionHeader::Serialize() const {
        if (algorithmInfo.size() > std::numeric_limits<std::uint16_t>::max()) {
                return {};
        }

        SerializedData output;
        std::size_t offset = 0;
        if (!SerializationUtils::WriteUnsigned(output, offset, Magic) ||
                !SerializationUtils::WriteUnsigned(output, offset, Version) ||
                !SerializationUtils::WriteUnsigned(output, offset, algorithm) ||
                !SerializationUtils::WriteUnsigned(
                        output, offset, static_cast<std::uint16_t>(algorithmInfo.size()))) {
                return {};
        }

        if (offset > output.max_size() ||
                algorithmInfo.size() > output.max_size() - offset) {
                return {};
        }
        output.insert(output.end(), algorithmInfo.begin(), algorithmInfo.end());
        return output;
}

bool EncryptionHeader::Deserialize(const SerializedData& data) {
        std::size_t offset = 0;
        std::uint16_t magic = 0;
        std::uint16_t version = 0;
        std::uint16_t parsedAlgorithm = 0;
        std::uint16_t infoLength = 0;
        if (!SerializationUtils::ReadUnsigned(data, offset, magic) || magic != Magic ||
                !SerializationUtils::ReadUnsigned(data, offset, version) || version != Version ||
                !SerializationUtils::ReadUnsigned(data, offset, parsedAlgorithm) ||
                !SerializationUtils::ReadUnsigned(data, offset, infoLength)) {
                return false;
        }

        // Allow trailing data (ciphertext) after header
        if (offset > data.size() || infoLength > data.size() - offset) {
                return false;
        }
        std::vector<std::uint8_t> parsedInfo(data.begin() + offset,
                                 data.begin() + offset + infoLength);

        algorithm = parsedAlgorithm;
        algorithmInfo = std::move(parsedInfo);
        headerSize = offset + infoLength;
        return true;
}
