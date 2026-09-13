#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class EncryptionHeader {
public:
	using SerializedData = std::vector<std::uint8_t>;

	static constexpr std::uint16_t Magic = 2;
	static constexpr std::uint16_t Version = 1;

	EncryptionHeader() = default;

	std::uint16_t Algorithm() const noexcept;
	void SetAlgorithm(std::uint16_t algorithm) noexcept;

	const std::vector<std::uint8_t>& AlgorithmInfo() const noexcept;
	void SetAlgorithmInfo(const std::vector<std::uint8_t>& info);

	std::size_t HeaderSize() const noexcept;

	SerializedData Serialize() const;
	bool Deserialize(const SerializedData& data);

private:
	std::uint16_t algorithm = 0;
	std::vector<std::uint8_t> algorithmInfo;
	std::size_t headerSize = 0;
};