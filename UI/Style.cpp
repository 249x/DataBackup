#include "Style.h"

#include <QString>

// 一套浅色主题：灰底、白卡片、蓝色主色（#1668dc）。
// 选择器都按控件类型或 objectName 写，界面代码里不用再散落 setStyleSheet。
namespace {

const char* const kStyle = R"QSS(
QWidget {
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 10pt;
    color: #1f2329;
}
QMainWindow, QDialog, QTabWidget::pane {
    background: #f5f6f8;
}
QTabWidget::pane {
    border: 1px solid #dcdfe6;
    border-radius: 8px;
    background: #ffffff;
    top: -1px;
}
QTabBar::tab {
    padding: 8px 24px;
    margin-right: 2px;
    border: none;
    color: #5b6470;
    background: transparent;
}
QTabBar::tab:selected {
    color: #1668dc;
    font-weight: 600;
    border-bottom: 2px solid #1668dc;
}
QTabBar::tab:hover {
    color: #1668dc;
}
QScrollArea {
    border: none;
    background: transparent;
}
QGroupBox {
    background: #ffffff;
    border: 1px solid #e3e6eb;
    border-radius: 8px;
    margin-top: 16px;
    padding: 14px 12px 12px 12px;
    font-weight: 600;
    color: #354052;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
}
QLineEdit, QComboBox, QPlainTextEdit {
    background: #ffffff;
    border: 1px solid #d5d9e0;
    border-radius: 6px;
    padding: 6px 8px;
    selection-background-color: #1668dc;
}
QLineEdit:focus, QComboBox:focus {
    border: 1px solid #1668dc;
}
QLineEdit:disabled, QComboBox:disabled {
    background: #f2f3f5;
    color: #a3a8b3;
}
QComboBox::drop-down {
    border: none;
    width: 22px;
}
QPushButton {
    background: #ffffff;
    border: 1px solid #d5d9e0;
    border-radius: 6px;
    padding: 6px 16px;
}
QPushButton:hover {
    border-color: #1668dc;
    color: #1668dc;
}
QPushButton:pressed {
    background: #eef4ff;
}
QPushButton#primary {
    background: #1668dc;
    border: 1px solid #1668dc;
    color: #ffffff;
    font-weight: 600;
}
QPushButton#primary:hover {
    background: #2b7ae4;
}
QPushButton#primary:pressed {
    background: #1257b8;
}
QPushButton#primary:disabled {
    background: #b9c6d8;
    border-color: #b9c6d8;
}
QLabel#preview {
    background: #eef4ff;
    border: 1px solid #cfe0ff;
    border-radius: 6px;
    padding: 8px 12px;
    color: #1c4e9c;
}
QLabel#hint {
    color: #6b7280;
}
QPlainTextEdit#log {
    font-family: Consolas, "DejaVu Sans Mono", monospace;
    font-size: 9.5pt;
    background: #1e1f22;
    color: #d6d9e0;
    border: 1px solid #2b2d31;
    border-radius: 8px;
}
QTableWidget {
    background: #ffffff;
    border: 1px solid #e3e6eb;
    border-radius: 8px;
    gridline-color: #eef0f3;
}
QHeaderView::section {
    background: #fafbfc;
    border: none;
    border-bottom: 1px solid #e3e6eb;
    padding: 8px;
    font-weight: 600;
    color: #4b5563;
}
QTableWidget::item:selected {
    background: #eef4ff;
    color: #1c4e9c;
}
QProgressBar {
    border: none;
    background: #e8ebf0;
    border-radius: 4px;
    height: 8px;
}
QProgressBar::chunk {
    background: #1668dc;
    border-radius: 4px;
}
QStatusBar {
    color: #6b7280;
}
)QSS";

} // namespace

QString Style::Sheet() {
    return QString::fromUtf8(kStyle);
}
