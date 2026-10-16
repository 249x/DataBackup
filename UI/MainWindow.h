#pragma once

#include <QMainWindow>
#include <QString>

#include <functional>

class System;

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QTimer;
class QWidget;

// 图形界面主窗口。
//
// 设计原则：内核一行不改。所有能力都是去调 System 里已经注册好的那些 Manager，
// 操作结果从 Debug 的全局日志里读回来（它本身带锁，天生适合跨线程读）。
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(System& system, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    QWidget* BuildBackupTab();
    QWidget* BuildRestoreTab();
    QWidget* BuildFilterTab();
    QWidget* BuildLogTab();

    bool PushSettings(QString& error);
    void SyncSettingsToBackend();
    bool ApplySettingsToBackend(QString& error);
    bool LoadKey(QComboBox* source, QLineEdit* edit, unsigned short algorithmId, QString& error);

    void RefreshSettingsPreview();
    void RefreshRestoreInfo();
    void RefreshFilterSummary();
    void RefreshFilterTable();
    void AppendLog(const QString& text);

    void StartBackup();
    void StartRestore();

    void AddFilter();
    void EditSelectedFilter();
    void RemoveSelectedFilter();

    void RunJob(const QString& title, const std::function<bool()>& work,
                QProgressBar* bar, QLabel* status);

    System& sys;

    QTabWidget* tabs = nullptr;

    // 备份页
    QLineEdit* backupSrc = nullptr;
    QLineEdit* backupDst = nullptr;
    QCheckBox* optArchive = nullptr;
    QCheckBox* optCompress = nullptr;
    QComboBox* compressAlgo = nullptr;
    QCheckBox* optEncrypt = nullptr;
    QComboBox* encryptAlgo = nullptr;
    QComboBox* keySource = nullptr;
    QLineEdit* keyEdit = nullptr;
    QPushButton* keyBrowse = nullptr;
    QLabel* settingsPreview = nullptr;
    QLabel* filterSummary = nullptr;
    QPushButton* backupRun = nullptr;
    QProgressBar* backupBar = nullptr;
    QLabel* backupStatus = nullptr;

    // 还原页
    QLineEdit* restoreSrc = nullptr;
    QLineEdit* restoreDst = nullptr;
    QLabel* restoreInfo = nullptr;
    QTimer* restoreInfoTimer = nullptr;
    QComboBox* restoreKeySource = nullptr;
    QLineEdit* restoreKeyEdit = nullptr;
    QPushButton* restoreKeyBrowse = nullptr;
    QPushButton* restoreRun = nullptr;
    QProgressBar* restoreBar = nullptr;
    QLabel* restoreStatus = nullptr;

    // 从备份文件头里识别出来的信息
    bool backupEncrypted = false;
    unsigned short backupEncryptAlgorithm = 0;

    // 筛选页
    QTableWidget* filterTable = nullptr;
    QLabel* filterCount = nullptr;

    // 日志页
    QPlainTextEdit* logView = nullptr;

    // 启动时把流水线（可能刚从存档读回来）的值灌进控件。这段时间里控件的信号不算
    // "用户改动"：控件是一个个同步的，同步到一半时推回流水线会把还没同步的那些
    // 控件（仍是默认值）当成用户设置写进去，刚读出来的设置就被覆盖了
    bool syncingSettings = false;

    bool busy = false;
};
