#pragma once

#include <cstdint>
#include <string>

// 一个算法的对外身份：
//   id           编号——会写进归档（StructuredFileContent 的 custom 字段），必须稳定，不能随意改
//   name         显示名——只给界面看的，随时可以改
//   description  用途说明——一句话，界面直接拼进提示文字
//
// 由各 handler 自己提供（GetID() + Name() + Description()），管理器汇总成清单（Algorithms / IdOf）
struct AlgorithmInfo {
	std::uint16_t id = 0;
	std::string name;
	std::string description;
};
