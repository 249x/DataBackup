#pragma once

// UI 与内核之间唯一的薄适配层。
//
// 这里只做两件事：把内核的 UTF-8 / std::filesystem::path 和 Qt 的 QString 互转，
// 以及把管理器已经提供好的清单（算法、筛选器类型）转发给界面 —— 界面不自己抄一份，
// 后端新增算法或筛选器类型时前端一行都不用改。

#include "../Core/System.h"
#include "../Core/General/Debug.h"
#include "../Core/General/Utf8.h"
#include "../Core/Managers/FileIO/FileIOManager.h"
#include "../Core/Managers/Archive/ArchiveManager.h"
#include "../Core/Managers/Compression/CompressionManager.h"
#include "../Core/Managers/Encryption/EncryptionManager.h"
#include "../Core/Managers/Filter/FilterManager.h"
#include "../Core/Managers/Pipeline/FilePipelineManager.h"

#include <QString>

#include <filesystem>
#include <string>
#include <vector>

namespace Backend {

// ---------- 文本 / 路径转换：沿用项目的 Utf8 约定 ----------

inline QString Text(const std::string& utf8) {
    return QString::fromUtf8(utf8.c_str());
}

inline QString Text(const std::filesystem::path& path) {
    return Text(Utf8::FromPath(path));
}

inline std::filesystem::path Path(const QString& text) {
    return Utf8::ToPath(text.toStdString());
}

// ---------- 算法清单 ----------
//
// 清单由内核提供：每个 handler 自己报编号、显示名与用途说明（GetID() / GetAlgorithmID() +
// Name() + Description()），管理器汇总成 AlgorithmInfo 列表。这里只做转发，
// 所以后端新增一个算法时前端一行都不用改。

inline std::vector<AlgorithmInfo> CompressionAlgorithms(System& sys) {
    CompressionManager* manager = sys.Get<CompressionManager>();
    return manager != nullptr ? manager->Algorithms() : std::vector<AlgorithmInfo>{};
}

inline std::vector<AlgorithmInfo> EncryptionAlgorithms(System& sys) {
    EncryptionManager* manager = sys.Get<EncryptionManager>();
    return manager != nullptr ? manager->Algorithms() : std::vector<AlgorithmInfo>{};
}

// 密钥长度也问后端（handler 的 MinimumKeySize()），界面不再自己写 32 / 8
inline int RequiredKeySize(System& sys, unsigned short id) {
    EncryptionManager* manager = sys.Get<EncryptionManager>();
    return manager != nullptr ? static_cast<int>(manager->KeySizeOf(id)) : 0;
}

// ---------- 筛选器类型清单 ----------
//
// 同样由内核提供：类型在注册时就带着界面信息存进 FilterManager（见 FilterTypeInfo.h），
// 这里只做转发。**顺序就是注册顺序**，FilterDialog 的类型索引依赖它。

inline std::vector<FilterTypeInfo> FilterTypeInfos(System& sys) {
    FilterManager* manager = sys.Get<FilterManager>();
    return manager != nullptr ? manager->TypeInfos() : std::vector<FilterTypeInfo>{};
}

// ---------- 日志 ----------
//
// 内核的 Debug 已经是带锁的全局日志，操作前 Clear、操作后读回来就是结构化结果，
// 所以界面完全不需要去解析 stdout。

inline QString LogText() {
    QString out;
    for (const ErrorEntry& entry : Debug::GetAllErrors()) {
        out += QStringLiteral("[%1] [%2] %3\n")
                   .arg(Text(Debug::LevelToString(entry.level)), Text(entry.tag),
                        Text(entry.message));
    }
    return out;
}

inline bool HasProblem() {
    return Debug::HasErrorsAbove(ErrorLevel::Warning);
}

} // namespace Backend
