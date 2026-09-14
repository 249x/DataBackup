#pragma once

#include <cstdint>
#include <vector>

/*
基本文件结构
uint16 magic; // 文件魔数
uint16 version; // 版本号
uint16 operation; // 操作类型
uint16 custom; // 自定义位
uint32 size; // 数据大小
uint8*size data; // 数据
*/

class StructuredFileContent {
public:
	static constexpr std::uint16_t Version = 1;
    static constexpr std::uint16_t Magic = 0xABCD;

	StructuredFileContent();
    StructuredFileContent(std::vector<std::uint8_t>& data, uint16_t operation, uint16_t sign);
    ~StructuredFileContent();

    const std::vector<std::uint8_t>& Data() const;
    std::vector<std::uint8_t>& Data();
    void SetData(const std::vector<std::uint8_t>& data);
    void SetData(std::vector<std::uint8_t>& data);

	bool Serialize(std::vector<std::uint8_t>& data);
	bool Deserialize(const std::vector<std::uint8_t>& data);

    std::uint16_t Operation();
    void SetOperation(std::uint16_t operation);

    const std::uint16_t& CustomRef() const;
    std::uint16_t& CustomRef();
    void SetCustom(std::uint16_t sign);

    std::uint16_t GetMagic();
    std::uint16_t GetVersion();
    std::uint16_t GetHeaderSizeBytes();

private:
    std::uint16_t operation;
    std::uint16_t custom;
    std::vector<std::uint8_t> data;

    bool SerializeHeader(std::vector<std::uint8_t>& data, std::size_t& offset);
    bool DeserializeHeader(const std::vector<std::uint8_t>& data, std::size_t& offset);
};
