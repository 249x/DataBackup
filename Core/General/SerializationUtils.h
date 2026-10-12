#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

class SerializationUtils {
public:
	using SerializedData = std::vector<std::uint8_t>;
	static constexpr std::uint64_t MaxFieldSize = 1ULL << 40;

	template <typename T>
	static bool WriteUnsigned(SerializedData& output, std::size_t& offset, T value) {
		static_assert(std::is_unsigned_v<T>);
		if (offset > output.max_size() || sizeof(T) > output.max_size() - offset) {
			return false;
		}
		if (output.size() < offset + sizeof(T)) {
			output.resize(offset + sizeof(T));
		}
		for (std::size_t index = 0; index < sizeof(T); ++index) {
			output[offset++] = static_cast<std::uint8_t>(value >> (index * 8));
		}
		return true;
	}

	template <typename T>
	static bool ReadUnsigned(const SerializedData& input, std::size_t& offset, T& value) {
		static_assert(std::is_unsigned_v<T>);
		if (offset > input.size() || input.size() - offset < sizeof(T)) {
			return false;
		}
		value = 0;
		for (std::size_t index = 0; index < sizeof(T); ++index) {
			value |= static_cast<T>(input[offset++]) << (index * 8);
		}
		return true;
	}

	template <typename T>
	static bool WriteUnsigned(std::ostream& output, T value) {
		static_assert(std::is_unsigned_v<T>);
		for (std::size_t index = 0; index < sizeof(T); ++index) {
			output.put(static_cast<char>(value >> (index * 8)));
		}
		return output.good();
	}

	template <typename T>
	static bool ReadUnsigned(std::istream& input, T& value) {
		static_assert(std::is_unsigned_v<T>);
		value = 0;
		for (std::size_t index = 0; index < sizeof(T); ++index) {
			const int byte = input.get();
			if (byte == std::char_traits<char>::eof()) {
				return false;
			}
			value |= static_cast<T>(static_cast<unsigned char>(byte)) << (index * 8);
		}
		return true;
	}

	static bool WriteString(SerializedData& output, std::size_t& offset, const std::string& value);
	static bool ReadString(const SerializedData& input, std::size_t& offset, std::string& value);
	static bool WriteBytes(SerializedData& output, std::size_t& offset, const SerializedData& data);
	static bool ReadBytes(const SerializedData& input, std::size_t& offset, SerializedData& output);
	static bool WriteBytes(SerializedData& output, std::size_t& offset, const std::size_t size, const SerializedData& data);
	static bool ReadBytes(const SerializedData& input, std::size_t& offset, const std::size_t size, SerializedData& output);

	static bool WriteBytes(std::ostream& output, const std::uint8_t* data, std::uint64_t size);
	static bool ReadBytes(std::istream& input, SerializedData& data, std::uint64_t size);
};