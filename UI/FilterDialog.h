#pragma once

#include "../Core/Managers/Filter/FilterTypeInfo.h"

#include <QDialog>
#include <QString>

#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;

// 过滤器编辑对话框。
//
// 上半部分按类型可视化填参数；下半部分始终显示最终要提交的规格串，
// 也可以直接改规格串 —— 编辑已有筛选器时就是靠这个回填的，
// 因为内核的 set-filter 只能换参数、不能换类型，界面这边统一按「删旧 + 加新」处理。
class FilterDialog : public QDialog {
public:
    // 类型清单由调用方给（来自 FilterManager::TypeInfos()），对话框不自己去问内核
    explicit FilterDialog(QWidget* parent, std::vector<FilterTypeInfo> types);

    // 交给 FilterManager::CreateAndAdd 的规格串
    QString Spec() const;
    void SetSpec(const QString& spec);

private:
    void BuildUi();
    void Rebuild();
    QString Compose() const;
    int PageOfType(int typeIndex) const;

    // 类型清单，按注册顺序（下拉框顺序与页签索引都依赖它）
    std::vector<FilterTypeInfo> typeInfos;

    QComboBox* typeBox = nullptr;
    QStackedWidget* pages = nullptr;
    QLabel* hintLabel = nullptr;
    QPlainTextEdit* specText = nullptr;

    QLineEdit* nameEdit = nullptr;
    QLineEdit* pathEdit = nullptr;
    QLineEdit* ownerEdit = nullptr;
    std::vector<QCheckBox*> typeChecks;

    QComboBox* sizeOp = nullptr;
    QLineEdit* sizeA = nullptr;
    QLineEdit* sizeB = nullptr;

    QComboBox* timeField = nullptr;
    QComboBox* timeOp = nullptr;
    QLineEdit* timeA = nullptr;
    QLineEdit* timeB = nullptr;

    QPlainTextEdit* childText = nullptr;
};
