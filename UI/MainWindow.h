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

    // 把界面上的开关推给内核的各个 Manager
    bool ApplySettingsToBackend(QString& error);
    // 密钥来源控件由调用方给：备份页和还原页各有一套，互不干扰
    bool LoadKey(QComboBox* source, QLineEdit* edit, unsigned short algorithmId, QString& error);

    void RefreshSettingsPreview();
    // 读出备份文件头，告诉用户这个备份里做了哪些处理
    void RefreshRestoreInfo();
    void RefreshFilterSummary();
    void RefreshFilterTable();
    void AppendLog(const QString& text);

    void StartBackup();
    void StartRestore();

    void AddFilter();
    void EditSelectedFilter();
    void RemoveSelectedFilter();

    // 在后台线程跑内核操作，主线程继续响应界面
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

    // 从备份文件头里识别出来的信息（用于还原时判断要不要密钥）
    bool backupEncrypted = false;
    unsigned short backupEncryptAlgorithm = 0;

    // 筛选页
    QTableWidget* filterTable = nullptr;
    QLabel* filterCount = nullptr;

    // 日志页
    QPlainTextEdit* logView = nullptr;

    bool busy = false;
};
