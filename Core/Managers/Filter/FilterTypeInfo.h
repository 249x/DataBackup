#pragma once

#include <string>

// 一个过滤类型的界面信息，注册类型时必须一起给出，存在管理器里（FilterManager::TypeInfos）。
//
//   name     登记名——也是 spec 里冒号前那个词（"name:*.tmp" 的 "name"）
//   syntax   语法提示（"name:模式"）
//   example  例子（"name:*.tmp"）
//   hint     一句话说明
//
// 为什么放注册时填：类型表本身就是个工厂（名字 -> 创建函数），界面信息跟着创建函数一起登记，
struct FilterTypeInfo {
	std::string name;
	std::string syntax;
	std::string example;
	std::string hint;
};
