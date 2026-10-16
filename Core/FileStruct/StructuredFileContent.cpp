#include "StructuredFileContent.h"
#include "../General/SerializationUtils.h"

#include <limits>

StructuredFileContent::StructuredFileContent() = default;

StructuredFileContent::StructuredFileContent(std::vector<std::uint8_t>& data, uint16_t operation, uint16_t sign):
    data(std::move(data)), operation(operation), custom(sign){
}

StructuredFileContent::~StructuredFileContent() = default;

const std::vector<std::uint8_t>& StructuredFileContent::Data() const {
    return data;
}

std::vector<std::uint8_t>& StructuredFileContent::Data(){
    return data;
}

void StructuredFileContent::SetData(const std::vector<std::uint8_t>& data) {
    this->data = data;
}

void StructuredFileContent::SetData(std::vector<std::uint8_t>& data) {
    this->data = std::move(data);
}

bool StructuredFileContent::Serialize(std::vector<uint8_t>& output) {
    output.clear();
    SerializationUtils io;
    if (!SerializeHeader(output, io)) {
        return false;
    }
    if (io.Offset() > output.max_size() || data.size() > output.max_size() - io.Offset()) {
        return false;
    }
    output.insert(output.end(), data.begin(), data.end());
    return true;
}

bool StructuredFileContent::Deserialize(const std::vector<uint8_t>& input) {
    SerializationUtils io;
    if (!DeserializeHeader(input, io)) {
        return false;
    }
    data.assign(input.begin() + static_cast<std::ptrdiff_t>(io.Offset()),
                input.begin() + static_cast<std::ptrdiff_t>(io.Offset() + data.size()));
    return true;
}

std::uint16_t StructuredFileContent::GetMagic() {
    return Magic;
}

std::uint16_t StructuredFileContent::GetVersion() {
    return Version;
}

std::uint16_t StructuredFileContent::Operation(){
    return operation;
}

void StructuredFileContent::SetOperation(std::uint16_t operation){
    this->operation = operation;
}

const std::uint16_t& StructuredFileContent::CustomRef() const{
    return custom;
}

std::uint16_t& StructuredFileContent::CustomRef(){
    return custom;
}

void StructuredFileContent::SetCustom(std::uint16_t sign){
    this->custom = sign;
}

std::uint16_t StructuredFileContent::GetHeaderSizeBytes() {
    return sizeof(std::uint16_t) * 4 + sizeof(std::uint32_t);
}

bool StructuredFileContent::SerializeHeader(std::vector<std::uint8_t>& data, SerializationUtils& io) {
    if (this->data.size() > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    if (!io.WriteUnsigned(data, GetMagic()) ||
        !io.WriteUnsigned(data, GetVersion()) ||
        !io.WriteUnsigned(data, operation) ||
        !io.WriteUnsigned(data, custom) ||
        !io.WriteUnsigned(data, static_cast<std::uint32_t>(this->data.size()))) {
        return false;
    }
    return true;
}

bool StructuredFileContent::DeserializeHeader(const std::vector<std::uint8_t>& data, SerializationUtils& io) {
    std::uint16_t magic = 0;
    std::uint16_t version = 0;
    std::uint32_t size = 0;

    if (!io.ReadUnsigned(data, magic) ||
        !io.ReadUnsigned(data, version) ||
        !io.ReadUnsigned(data, operation) ||
        !io.ReadUnsigned(data, custom) ||
        !io.ReadUnsigned(data, size)) {
        return false;
    }

    if (magic != GetMagic() || version != Version) {
        return false;
    }

    if (size > SerializationUtils::MaxFieldSize ||
        io.Offset() > data.size() ||
        size > data.size() - io.Offset()) {
            return false;
        }
    this->data.resize(size);
    return true;
}

