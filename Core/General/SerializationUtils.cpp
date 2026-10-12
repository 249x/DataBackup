#include "SerializationUtils.h"

#include <algorithm>
#include <limits>

bool SerializationUtils::WriteString(SerializedData& output, std::size_t& offset,
                                     const std::string& value) {
	if (value.size() > MaxFieldSize ||
		value.size() > std::numeric_limits<std::uint64_t>::max()) {
		return false;
	}
	if (!WriteUnsigned<std::uint64_t>(
			output, offset, static_cast<std::uint64_t>(value.size()))) {
		return false;
	}
	if (offset > output.max_size() || value.size() > output.max_size() - offset) {
		return false;
	}
	if (output.size() < offset + value.size()) {
		output.resize(offset + value.size());
	}
	std::copy(value.begin(), value.end(), output.begin() + offset);
	offset += value.size();
	return true;
}

bool SerializationUtils::WriteBytes(SerializedData& output, std::size_t& offset,
                                    const SerializedData& data) {
	if (data.size() > MaxFieldSize ||
		!WriteUnsigned<std::uint64_t>(output, offset,
			static_cast<std::uint64_t>(data.size()))) {
		return false;
	}
	if (offset > output.max_size() || data.size() > output.max_size() - offset) {
		return false;
	}
	if (output.size() < offset + data.size()) {
		output.resize(offset + data.size());
	}
	std::copy(data.begin(), data.end(), output.begin() + offset);
	offset += data.size();
	return true;
}

bool SerializationUtils::ReadString(const SerializedData& input,
                                    std::size_t& offset, std::string& value) {
	std::uint64_t size = 0;
	if (!ReadUnsigned(input, offset, size) ||
		size > MaxFieldSize || size > input.size() - offset ||
		size > std::string().max_size()) {
		return false;
	}

	value.assign(reinterpret_cast<const char*>(input.data() + offset),
	             static_cast<std::size_t>(size));
	offset += static_cast<std::size_t>(size);
	return true;
}

bool SerializationUtils::ReadBytes(const SerializedData& input,
                                   std::size_t& offset, SerializedData& output) {
	std::uint64_t size = 0;
	if (!ReadUnsigned(input, offset, size) ||
		size > MaxFieldSize || size > input.size() - offset) {
		return false;
	}
	output.assign(input.begin() + static_cast<std::size_t>(offset),
	              input.begin() + static_cast<std::size_t>(offset + size));
	offset += static_cast<std::size_t>(size);
	return true;
}

bool SerializationUtils::WriteBytes(std::ostream& output,
                                    const std::uint8_t* data,
                                    std::uint64_t size) {
	if (size > MaxFieldSize) {
		return false;
	}
	if (size == 0) {
		return true;
	}
	output.write(reinterpret_cast<const char*>(data),
	             static_cast<std::streamsize>(size));
	return output.good();
}

bool SerializationUtils::ReadBytes(std::istream& input, SerializedData& data,
                                   std::uint64_t size) {
	if (size > MaxFieldSize || size > data.max_size() ||
		size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
		return false;
	}
	data.resize(static_cast<std::size_t>(size));
	if (size == 0) {
		return true;
	}
	input.read(reinterpret_cast<char*>(data.data()),
	           static_cast<std::streamsize>(size));
	return input.good();
}

bool SerializationUtils::WriteBytes(SerializedData& output, std::size_t& offset,
									const std::size_t size, const SerializedData& data) {
	if (size > MaxFieldSize ||
		size > std::numeric_limits<std::uint64_t>::max() ||
		!WriteUnsigned<std::uint64_t>(output, offset,
			static_cast<std::uint64_t>(size))) {
		return false;
	}
	if (offset > output.max_size() || size > output.max_size() - offset) {
		return false;
	}
	if (output.size() < offset + size) {
		output.resize(offset + size);
	}
	std::copy(data.begin(), data.begin() + size, output.begin() + offset);
	offset += size;
	return true;
}

bool SerializationUtils::ReadBytes(const SerializedData& input, std::size_t& offset,
								   const std::size_t size, SerializedData& output) {
	std::uint64_t readSize = 0;
	if (!ReadUnsigned(input, offset, readSize) ||
		readSize > MaxFieldSize || readSize > input.size() - offset ||
		readSize > std::numeric_limits<std::size_t>::max() ||
		readSize != size) {
		return false;
	}
	output.assign(input.begin() + static_cast<std::size_t>(offset),
	              input.begin() + static_cast<std::size_t>(offset + readSize));
	offset += static_cast<std::size_t>(readSize);
	return true;
}
