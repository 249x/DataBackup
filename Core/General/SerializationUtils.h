#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

/*
 * 序列化 / 反序列化小工具。
 *
 * 游标（offset）由对象自己管：**一次序列化用一个实例**，从头往后走，调用点不用再
 * 声明 `std::size_t offset = 0;` 并把它传给每一次调用。
 * 要复用同一个实例从头再来一遍，调用 Reset()；需要知道当前位置或用它做
 * “刚好读完”的校验时用 Offset()。
 */
class SerializationUtils {
public:
	using SerializedData = std::vector<std::uint8_t>;
	static constexpr std::uint64_t MaxFieldSize = 1ULL << 40;

	void Reset() noexcept { offset = 0; }
	std::size_t Offset() const noexcept { return offset; }

	template <typename T>
	bool WriteUnsigned(SerializedData& output, T value) {
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
	bool ReadUnsigned(const SerializedData& input, T& value) {
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

	bool WriteString(SerializedData& output, const std::string& value);
	bool ReadString(const SerializedData& input, std::string& value);
	bool WriteBytes(SerializedData& output, const SerializedData& data);
	bool ReadBytes(const SerializedData& input, SerializedData& output);
	bool WriteBytes(SerializedData& output, const std::size_t size, const SerializedData& data);
	bool ReadBytes(const SerializedData& input, const std::size_t size, SerializedData& output);

	static bool WriteBytes(std::ostream& output, const std::uint8_t* data, std::uint64_t size);
	static bool ReadBytes(std::istream& input, SerializedData& data, std::uint64_t size);

private:
	std::size_t offset = 0;
};