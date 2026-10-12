#include "MainWindow.h"

#include "Backend.h"
#include "FilterDialog.h"

#include <QAbstractItemView>
#include <QCryptographicHash>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextCursor>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kLogTabIndex = 3;

// 文件选择框的起始位置：优先顺着界面上已经填好的路径走
QString StartDir(const QLineEdit* edit) {
    const QString text = edit != nullptr ? edit->text().trimmed() : QString();
    if (!text.isEmpty()) {
        const QFileInfo info(text);
        if (info.isDir()) {
            return info.absoluteFilePath();
        }
        if (info.absoluteDir().exists()) {
            return info.absoluteDir().absolutePath();
        }
    }
    return QDir::homePath();
}

// 一行「输入框 + 浏览按钮」
QWidget* MakePathRow(QWidget* parent, QLineEdit* edit, const std::function<void()>& onBrowse) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* browse = new QPushButton(QStringLiteral("浏览…"), row);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, row, [onBrowse] { onBrowse(); });
    return row;
}

// 每个页签都套一层滚动区：窗口拉小时控件只会滚动，不会被裁掉。
QWidget* WrapScroll(QWidget* content) {
    auto* area = new QScrollArea();
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidget(content);
    return area;
}

// 每一层包装的头都是 StructuredFileContent 的序列化头部，12 字节小端：
//   uint16 magic(0xABCD) | uint16 version(1) | uint16 operation | uint16 custom | uint32 size
// operation: 0=无 1=打包 2=压缩 3=加密；custom 在 2/3 时是算法号（名字去问后端清单）。
struct BackupHeaderInfo {
    bool valid = false;
    quint16 operation = 0;
    quint16 custom = 0;
    quint32 size = 0; // 头后面跟着的负载长度
};

BackupHeaderInfo ParseLayerHeader(const QByteArray& bytes) {
    BackupHeaderInfo info;
    if (bytes.size() < 12) {
        return info;
    }
    const auto readU16 = [&bytes](int at) {
        return static_cast<quint16>(static_cast<quint8>(bytes[at])) |
               static_cast<quint16>(static_cast<quint8>(bytes[at + 1]) << 8);
    };
    const auto readU32 = [&bytes](int at) {
        return static_cast<quint32>(static_cast<quint8>(bytes[at])) |
               (static_cast<quint32>(static_cast<quint8>(bytes[at + 1])) << 8) |
               (static_cast<quint32>(static_cast<quint8>(bytes[at + 2])) << 16) |
               (static_cast<quint32>(static_cast<quint8>(bytes[at + 3])) << 24);
    };
    const quint16 magic = readU16(0);
    const quint16 version = readU16(2);
    info.operation = readU16(4);
    info.custom = readU16(6);
    info.size = readU32(8);
    info.valid = (magic == 0xABCD && version == 1);
    return info;
}

// 头固定 12 字节，后面才是负载。内核的解密/解压接口要的是负载本身，
// 不能把包头一起喂进去，否则会把包头当成 IV 用。
QByteArray PayloadOf(const QByteArray& body, const BackupHeaderInfo& layer) {
    constexpr int kHeaderSize = 12;
    if (body.size() <= kHeaderSize) {
        return QByteArray();
    }
    const int available = body.size() - kHeaderSize;
    const int length = layer.size <= static_cast<quint32>(available)
                           ? static_cast<int>(layer.size)
                           : available;
    return body.mid(kHeaderSize, length);
}

// 编号 -> 显示名：名字全部来自后端清单（handler → 管理器 → Backend），
// 这里只做查找；认不出的编号就照原样报出来。
QString AlgorithmName(const std::vector<AlgorithmInfo>& list, quint16 id) {
    for (const AlgorithmInfo& algo : list) {
        if (algo.id == id) {
            return QString::fromUtf8(algo.name.c_str());
        }
    }
    return QStringLiteral("未知算法 %1").arg(id);
}

// 下拉框旁边那条说明也由后端清单拼：名字与用途说明来自 handler，密钥长度来自加密管理器。
// 后端加新算法时这段文字自动跟着变，界面不用改。
QString AlgorithmHintText(System& sys) {
    QStringList parts;

    QStringList compression;
    for (const AlgorithmInfo& algo : Backend::CompressionAlgorithms(sys)) {
        compression << QStringLiteral("%1（%2）")
                           .arg(Backend::Text(algo.name), Backend::Text(algo.description));
    }
    if (!compression.isEmpty()) {
        parts << QStringLiteral("压缩：") + compression.join(QStringLiteral("，"));
    }

    QStringList encryption;
    for (const AlgorithmInfo& algo : Backend::EncryptionAlgorithms(sys)) {
        const int need = Backend::RequiredKeySize(sys, static_cast<unsigned short>(algo.id));
        encryption << QStringLiteral("%1（%2，密钥至少 %3 字节）")
                          .arg(Backend::Text(algo.name), Backend::Text(algo.description))
                          .arg(need);
    }
    if (!encryption.isEmpty()) {
        parts << QStringLiteral("加密：") + encryption.join(QStringLiteral("，"));
    }

    return parts.join(QStringLiteral("。")) + QStringLiteral("。");
}

std::vector<std::uint8_t> ToVector(const QByteArray& bytes) {
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

QByteArray ToBytes(const std::vector<std::uint8_t>& data) {
    return QByteArray(reinterpret_cast<const char*>(data.data()),
                      static_cast<int>(data.size()));
}

} // namespace

MainWindow::MainWindow(System& system, QWidget* parent)
    : QMainWindow(parent), sys(system) {
    setWindowTitle(QStringLiteral("数据备份工具  DataBackup"));
    resize(1060, 820);
    setMinimumSize(880, 600);

    tabs = new QTabWidget(this);
    tabs->addTab(WrapScroll(BuildBackupTab()), QStringLiteral("备份"));
    tabs->addTab(WrapScroll(BuildRestoreTab()), QStringLiteral("还原"));
    tabs->addTab(WrapScroll(BuildFilterTab()), QStringLiteral("筛选设置"));
    tabs->addTab(WrapScroll(BuildLogTab()), QStringLiteral("运行日志"));
    setCentralWidget(tabs);

    RefreshFilterSummary();
    RefreshFilterTable();
    RefreshSettingsPreview();
    RefreshRestoreInfo();
    statusBar()->showMessage(QStringLiteral("就绪"));
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------- 备份页

QWidget* MainWindow::BuildBackupTab() {
    auto* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);

    // ---- 路径设置 ----
    auto* pathBox = new QGroupBox(QStringLiteral("路径设置"), page);
    auto* pathForm = new QFormLayout(pathBox);

    backupSrc = new QLineEdit(pathBox);
    backupSrc->setObjectName(QStringLiteral("backupSrc"));
    backupSrc->setPlaceholderText(QStringLiteral("要备份的目录，例如 D:/data"));
    pathForm->addRow(QStringLiteral("源目录"),
                     MakePathRow(pathBox, backupSrc, [this] {
                         const QString dir = QFileDialog::getExistingDirectory(
                             this, QStringLiteral("选择要备份的目录"), StartDir(backupSrc));
                         if (!dir.isEmpty()) {
                             backupSrc->setText(QDir::toNativeSeparators(dir));
                         }
                     }));

    backupDst = new QLineEdit(pathBox);
    backupDst->setObjectName(QStringLiteral("backupDst"));
    backupDst->setPlaceholderText(QStringLiteral("备份文件保存位置，例如 D:/backup/demo.db"));
    pathForm->addRow(QStringLiteral("备份到"),
                     MakePathRow(pathBox, backupDst, [this] {
                         const QString file = QFileDialog::getSaveFileName(
                             this, QStringLiteral("选择备份文件"), StartDir(backupDst),
                             QStringLiteral("备份文件 (*.db);;所有文件 (*)"));
                         if (!file.isEmpty()) {
                             backupDst->setText(QDir::toNativeSeparators(file));
                         }
                     }));
    root->addWidget(pathBox);

    // ---- 处理流程 ----
    auto* optBox = new QGroupBox(
        QStringLiteral("处理流程　（内核固定按 打包 → 压缩 → 加密 的顺序执行）"), page);
    auto* optGrid = new QGridLayout(optBox);
    optGrid->setHorizontalSpacing(12);

    optArchive = new QCheckBox(QStringLiteral("打包：把整棵目录树拼成单个归档文件"), optBox);
    optArchive->setChecked(true); // 与 ArchiveManager 的默认值一致
    optGrid->addWidget(optArchive, 0, 0, 1, 3);

    optCompress = new QCheckBox(QStringLiteral("压缩"), optBox);
    compressAlgo = new QComboBox(optBox);
    for (const AlgorithmInfo& algo : Backend::CompressionAlgorithms(sys)) {
        compressAlgo->addItem(Backend::Text(algo.name), algo.id);
    }
    compressAlgo->setMinimumWidth(160);
    optGrid->addWidget(optCompress, 1, 0);
    optGrid->addWidget(compressAlgo, 1, 1);
    optGrid->setColumnStretch(2, 1);

    optEncrypt = new QCheckBox(QStringLiteral("加密"), optBox);
    encryptAlgo = new QComboBox(optBox);
    for (const AlgorithmInfo& algo : Backend::EncryptionAlgorithms(sys)) {
        encryptAlgo->addItem(Backend::Text(algo.name), algo.id);
    }
    encryptAlgo->setMinimumWidth(160);
    optGrid->addWidget(optEncrypt, 2, 0);
    optGrid->addWidget(encryptAlgo, 2, 1);

    // 密钥来源：文件 或 口令
    keySource = new QComboBox(optBox);
    keySource->addItems({QStringLiteral("密钥文件"), QStringLiteral("口令")});
    keyEdit = new QLineEdit(optBox);
    keyEdit->setObjectName(QStringLiteral("keyEdit"));
    keyEdit->setPlaceholderText(QStringLiteral("选择密钥文件"));
    QWidget* keyRow = MakePathRow(optBox, keyEdit, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择密钥文件"), StartDir(keyEdit),
            QStringLiteral("所有文件 (*)"));
        if (!file.isEmpty()) {
            keyEdit->setText(QDir::toNativeSeparators(file));
        }
    });
    keyBrowse = keyRow->findChild<QPushButton*>();

    auto* keyLine = new QWidget(optBox);
    auto* keyLayout = new QHBoxLayout(keyLine);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(new QLabel(QStringLiteral("密钥来源"), keyLine));
    keyLayout->addWidget(keySource);
    keyLayout->addWidget(keyRow, 1);
    optGrid->addWidget(keyLine, 3, 0, 1, 3);

    // 提示文字单独占一行并允许换行，否则它会成为整页的最小宽度，把布局撑出窗口
    auto* optionHint = new QLabel(AlgorithmHintText(sys), optBox);
    optionHint->setObjectName(QStringLiteral("hint"));
    optionHint->setWordWrap(true);
    optGrid->addWidget(optionHint, 4, 0, 1, 3);
    root->addWidget(optBox);

    // ---- 当前设置预览 ----
    settingsPreview = new QLabel(page);
    settingsPreview->setObjectName(QStringLiteral("preview"));
    settingsPreview->setWordWrap(true);
    root->addWidget(settingsPreview);

    // ---- 筛选摘要 ----
    auto* filterBox = new QGroupBox(QStringLiteral("自定义备份（筛选）"), page);
    auto* filterLayout = new QHBoxLayout(filterBox);
    filterSummary = new QLabel(filterBox);
    filterSummary->setWordWrap(true);
    auto* gotoFilter = new QPushButton(QStringLiteral("设置筛选条件…"), filterBox);
    filterLayout->addWidget(filterSummary, 1);
    filterLayout->addWidget(gotoFilter);
    root->addWidget(filterBox);
    connect(gotoFilter, &QPushButton::clicked, this, [this] { tabs->setCurrentIndex(2); });

    // ---- 执行 ----
    auto* actionRow = new QHBoxLayout();
    backupRun = new QPushButton(QStringLiteral("开始备份"), page);
    backupRun->setObjectName(QStringLiteral("primary"));
    backupRun->setMinimumHeight(40);
    backupRun->setMinimumWidth(140);
    backupBar = new QProgressBar(page);
    backupBar->setRange(0, 0);
    backupBar->setVisible(false);
    backupBar->setFixedWidth(200);
    backupStatus = new QLabel(QStringLiteral("尚未执行"), page);
    actionRow->addWidget(backupRun);
    actionRow->addWidget(backupBar);
    actionRow->addWidget(backupStatus, 1);
    root->addLayout(actionRow);
    root->addStretch(1);

    connect(backupRun, &QPushButton::clicked, this, [this] { StartBackup(); });
    connect(optArchive, &QCheckBox::toggled, this, [this] { RefreshSettingsPreview(); });
    connect(optCompress, &QCheckBox::toggled, this, [this] { RefreshSettingsPreview(); });
    connect(optEncrypt, &QCheckBox::toggled, this, [this] { RefreshSettingsPreview(); });
    connect(compressAlgo, &QComboBox::currentIndexChanged, this, [this] { RefreshSettingsPreview(); });
    connect(encryptAlgo, &QComboBox::currentIndexChanged, this, [this] { RefreshSettingsPreview(); });
    connect(keySource, &QComboBox::currentIndexChanged, this, [this](int index) {
        keyEdit->setEchoMode(index == 0 ? QLineEdit::Normal : QLineEdit::Password);
        keyEdit->setText(QString());
        keyEdit->setPlaceholderText(index == 0
                                        ? QStringLiteral("选择密钥文件")
                                        : QStringLiteral("输入口令，界面会用 SHA-256 派生密钥"));
        RefreshSettingsPreview();
    });

    return page;
}

// ---------------------------------------------------------------- 还原页

QWidget* MainWindow::BuildRestoreTab() {
    auto* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);

    auto* box = new QGroupBox(QStringLiteral("路径设置"), page);
    auto* form = new QFormLayout(box);

    restoreSrc = new QLineEdit(box);
    restoreSrc->setObjectName(QStringLiteral("restoreSrc"));
    restoreSrc->setPlaceholderText(QStringLiteral("之前生成的备份文件"));
    form->addRow(QStringLiteral("备份文件"),
                 MakePathRow(box, restoreSrc, [this] {
                     const QString file = QFileDialog::getOpenFileName(
                         this, QStringLiteral("选择备份文件"), StartDir(restoreSrc),
                         QStringLiteral("备份文件 (*.db);;所有文件 (*)"));
                     if (!file.isEmpty()) {
                         restoreSrc->setText(QDir::toNativeSeparators(file));
                     }
                 }));

    restoreDst = new QLineEdit(box);
    restoreDst->setObjectName(QStringLiteral("restoreDst"));
    restoreDst->setPlaceholderText(QStringLiteral("还原到哪里（建议每次换一个新目录）"));
    form->addRow(QStringLiteral("输出目录"),
                 MakePathRow(box, restoreDst, [this] {
                     const QString dir = QFileDialog::getExistingDirectory(
                         this, QStringLiteral("选择输出目录"), StartDir(restoreDst));
                     if (!dir.isEmpty()) {
                         restoreDst->setText(QDir::toNativeSeparators(dir));
                     }
                 }));
    root->addWidget(box);

    // ---- 备份内容自动识别 ----
    auto* infoBox = new QGroupBox(QStringLiteral("备份内容（自动识别）"), page);
    auto* infoLayout = new QVBoxLayout(infoBox);
    restoreInfo = new QLabel(QStringLiteral("选择备份文件后会自动识别里面的处理方式。"), infoBox);
    restoreInfo->setWordWrap(true);
    restoreInfo->setObjectName(QStringLiteral("hint"));
    infoLayout->addWidget(restoreInfo);
    root->addWidget(infoBox);

    // ---- 解密设置 ----
    // 打包和压缩不用这里配置：备份时做了什么、用的哪个算法，都记在备份文件头里，
    // 还原时由内核直接按文件里记的走。只有密钥不在文件里，必须由用户提供。
    auto* keyBox = new QGroupBox(QStringLiteral("解密设置（仅当备份已加密时才需要）"), page);
    auto* keyForm = new QFormLayout(keyBox);

    restoreKeySource = new QComboBox(keyBox);
    restoreKeySource->addItems({QStringLiteral("密钥文件"), QStringLiteral("口令")});
    restoreKeyEdit = new QLineEdit(keyBox);
    restoreKeyEdit->setObjectName(QStringLiteral("restoreKeyEdit"));
    restoreKeyEdit->setPlaceholderText(QStringLiteral("备份时用的密钥文件"));
    QWidget* restoreKeyRow = MakePathRow(keyBox, restoreKeyEdit, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择密钥文件"), StartDir(restoreKeyEdit),
            QStringLiteral("所有文件 (*)"));
        if (!file.isEmpty()) {
            restoreKeyEdit->setText(QDir::toNativeSeparators(file));
        }
    });
    restoreKeyBrowse = restoreKeyRow->findChild<QPushButton*>();

    auto* restoreKeyLine = new QWidget(keyBox);
    auto* restoreKeyLayout = new QHBoxLayout(restoreKeyLine);
    restoreKeyLayout->setContentsMargins(0, 0, 0, 0);
    restoreKeyLayout->addWidget(new QLabel(QStringLiteral("密钥来源"), restoreKeyLine));
    restoreKeyLayout->addWidget(restoreKeySource);
    restoreKeyLayout->addWidget(restoreKeyRow, 1);
    keyForm->addRow(restoreKeyLine);

    auto* keyHint = new QLabel(
        QStringLiteral("必须与备份时完全一致。密钥不对会被识别出来并以「解密失败」终止，"
                       "不会产出乱码文件；留空则会被下面的检查直接拦住。"),
        keyBox);
    keyHint->setObjectName(QStringLiteral("hint"));
    keyHint->setWordWrap(true);
    keyForm->addRow(keyHint);
    root->addWidget(keyBox);

    connect(restoreKeySource, &QComboBox::currentIndexChanged, this, [this](int index) {
        restoreKeyEdit->setEchoMode(index == 0 ? QLineEdit::Normal : QLineEdit::Password);
        restoreKeyEdit->setText(QString());
        restoreKeyEdit->setPlaceholderText(index == 0
                                               ? QStringLiteral("备份时用的密钥文件")
                                               : QStringLiteral("备份时用的口令"));
        if (restoreKeyBrowse != nullptr) {
            restoreKeyBrowse->setEnabled(index == 0);
        }
    });
    // 识别可能会去解密/解压，开销不小，所以做 350ms 防抖，
    // 避免每敲一个字符就整份文件跑一遍
    restoreInfoTimer = new QTimer(this);
    restoreInfoTimer->setSingleShot(true);
    connect(restoreInfoTimer, &QTimer::timeout, this, [this] { RefreshRestoreInfo(); });

    // 备份文件或密钥一变就重新识别：填上密钥后，加密层里面的内容才看得见
    connect(restoreSrc, &QLineEdit::textChanged, this,
            [this] { restoreInfoTimer->start(350); });
    connect(restoreKeyEdit, &QLineEdit::textChanged, this,
            [this] { restoreInfoTimer->start(350); });

    auto* note = new QLabel(
        QStringLiteral("提示：内核的还原是「往目录里写」，不会清理目标目录里已有的文件。\n"
                       "如果要验证还原结果，请选一个空目录，否则会混进上一次的残留文件。"),
        page);
    note->setObjectName(QStringLiteral("hint"));
    note->setWordWrap(true);
    root->addWidget(note);

    auto* actionRow = new QHBoxLayout();
    restoreRun = new QPushButton(QStringLiteral("开始还原"), page);
    restoreRun->setObjectName(QStringLiteral("primary"));
    restoreRun->setMinimumHeight(40);
    restoreRun->setMinimumWidth(140);
    restoreBar = new QProgressBar(page);
    restoreBar->setRange(0, 0);
    restoreBar->setVisible(false);
    restoreBar->setFixedWidth(200);
    restoreStatus = new QLabel(QStringLiteral("尚未执行"), page);
    actionRow->addWidget(restoreRun);
    actionRow->addWidget(restoreBar);
    actionRow->addWidget(restoreStatus, 1);
    root->addLayout(actionRow);
    root->addStretch(1);

    connect(restoreRun, &QPushButton::clicked, this, [this] { StartRestore(); });
    return page;
}

// ---------------------------------------------------------------- 筛选页

QWidget* MainWindow::BuildFilterTab() {
    auto* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);
    root->setSpacing(10);
    root->setContentsMargins(16, 16, 16, 16);

    auto* note = new QLabel(
        QStringLiteral("筛选是白名单：下面每一条都必须通过，条目才会被备份；不添加任何条件时备份全部。\n"
                       "想表达「排除 .tmp」要写成 not:(name:*.tmp)，而不是 name:*.tmp。"),
        page);
    note->setObjectName(QStringLiteral("hint"));
    note->setWordWrap(true);
    root->addWidget(note);

    filterTable = new QTableWidget(page);
    filterTable->setColumnCount(3);
    filterTable->setHorizontalHeaderLabels(
        {QStringLiteral("编号"), QStringLiteral("规格"), QStringLiteral("状态")});
    filterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    filterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    filterTable->setSelectionMode(QAbstractItemView::SingleSelection);
    filterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    filterTable->verticalHeader()->setVisible(false);
    root->addWidget(filterTable, 1);

    auto* row = new QHBoxLayout();
    auto* addBtn = new QPushButton(QStringLiteral("添加条件…"), page);
    auto* editBtn = new QPushButton(QStringLiteral("编辑所选…"), page);
    auto* removeBtn = new QPushButton(QStringLiteral("删除所选"), page);
    filterCount = new QLabel(page);
    row->addWidget(addBtn);
    row->addWidget(editBtn);
    row->addWidget(removeBtn);
    row->addStretch(1);
    row->addWidget(filterCount);
    root->addLayout(row);

    connect(addBtn, &QPushButton::clicked, this, [this] { AddFilter(); });
    connect(editBtn, &QPushButton::clicked, this, [this] { EditSelectedFilter(); });
    connect(removeBtn, &QPushButton::clicked, this, [this] { RemoveSelectedFilter(); });
    connect(filterTable, &QTableWidget::doubleClicked, this, [this] { EditSelectedFilter(); });
    return page;
}

// ---------------------------------------------------------------- 日志页

QWidget* MainWindow::BuildLogTab() {
    auto* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);
    root->setSpacing(10);
    root->setContentsMargins(16, 16, 16, 16);

    auto* note = new QLabel(
        QStringLiteral("这里显示内核 Debug 日志的全部内容，和命令行里看到的 [INFO]/[WARNING]/[ERROR] 是同一条。"),
        page);
    note->setObjectName(QStringLiteral("hint"));
    note->setWordWrap(true);
    root->addWidget(note);

    logView = new QPlainTextEdit(page);
    logView->setReadOnly(true);
    logView->setObjectName(QStringLiteral("log"));
    root->addWidget(logView, 1);

    auto* row = new QHBoxLayout();
    auto* clearBtn = new QPushButton(QStringLiteral("清空"), page);
    auto* saveBtn = new QPushButton(QStringLiteral("导出到文件…"), page);
    row->addWidget(clearBtn);
    row->addWidget(saveBtn);
    row->addStretch(1);
    root->addLayout(row);

    connect(clearBtn, &QPushButton::clicked, this, [this] { logView->clear(); });
    connect(saveBtn, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出日志"), QDir::homePath() + QStringLiteral("/databackup.log"),
            QStringLiteral("日志文件 (*.log);;文本文件 (*.txt)"));
        if (file.isEmpty()) {
            return;
        }
        QFile out(file);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入该文件。"));
            return;
        }
        out.write(logView->toPlainText().toUtf8());
    });
    return page;
}

// ---------------------------------------------------------------- 内核调用

bool MainWindow::ApplySettingsToBackend(QString& error) {
    auto* archive = sys.Get<ArchiveManager>();
    auto* compression = sys.Get<CompressionManager>();
    auto* encryption = sys.Get<EncryptionManager>();
    if (archive == nullptr || compression == nullptr || encryption == nullptr) {
        error = QStringLiteral("内核 Manager 未初始化。");
        return false;
    }

    archive->SetArchive(optArchive->isChecked());

    const auto compressId = static_cast<unsigned short>(compressAlgo->currentData().toUInt());
    if (!compression->SetCompression(optCompress->isChecked(), compressId)) {
        error = QStringLiteral("压缩算法无效。");
        return false;
    }

    const auto encryptId = static_cast<unsigned short>(encryptAlgo->currentData().toUInt());
    if (!encryption->SetEncryption(optEncrypt->isChecked(), encryptId)) {
        error = QStringLiteral("加密算法无效。");
        return false;
    }

    if (optEncrypt->isChecked() &&
        !LoadKey(keySource, keyEdit, encryptId, error)) {
        return false;
    }
    return true;
}

bool MainWindow::LoadKey(QComboBox* source, QLineEdit* edit,
                         unsigned short algorithmId, QString& error) {
    auto* encryption = sys.Get<EncryptionManager>();
    const QString text = edit->text();
    if (text.trimmed().isEmpty()) {
        error = QStringLiteral("请先指定密钥文件或输入口令。");
        return false;
    }

    // 密钥文件：内核本来就有这条路径，直接用
    if (source->currentIndex() == 0) {
        if (!encryption->SetKeyPath(Backend::Path(text))) {
            error = QStringLiteral("读取密钥文件失败：") + text;
            return false;
        }
        return true;
    }

    // 口令：内核只接受「密钥文件」这一种输入，所以这里在界面侧把口令做
    // SHA-256 派生，写成临时文件喂给 SetKeyPath，读完立刻删掉。
    // 这样做是为了不改内核；密钥长度按所选算法取（AES 32 字节 / DES 8 字节）。
    const int need = Backend::RequiredKeySize(sys, algorithmId);
    const QByteArray digest =
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
    const QByteArray key(digest.constData(), need);

    const QString tempPath = QDir::tempPath() + QStringLiteral("/databackup_key.tmp");
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("无法写入临时密钥文件：") + tempPath;
        return false;
    }
    tempFile.write(key);
    tempFile.close();

    const bool ok = encryption->SetKeyPath(Backend::Path(tempPath));
    QFile::remove(tempPath);
    if (!ok) {
        error = QStringLiteral("加载派生密钥失败。");
        return false;
    }
    return true;
}

void MainWindow::RefreshRestoreInfo() {
    const QString path = restoreSrc->text().trimmed();
    backupEncrypted = false;
    backupEncryptAlgorithm = 0;

    if (path.isEmpty()) {
        restoreInfo->setStyleSheet(QStringLiteral("color:#6b7280;"));
        restoreInfo->setText(QStringLiteral("选择备份文件后会自动识别里面的处理方式。"));
        return;
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        restoreInfo->setStyleSheet(QStringLiteral("color:#b45309;"));
        restoreInfo->setText(QStringLiteral("找不到这个路径。"));
        return;
    }
    if (fileInfo.isDir()) {
        // 关掉"打包"时备份产物就是一个目录，里面是直接保存的原始文件
        restoreInfo->setStyleSheet(QStringLiteral("color:#6b7280;"));
        restoreInfo->setText(QStringLiteral(
            "这是一个未打包的备份目录：文件按原目录结构直接存放，还原时照原样写回。"));
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        restoreInfo->setStyleSheet(QStringLiteral("color:#b45309;"));
        restoreInfo->setText(QStringLiteral("无法读取这个文件（可能被占用或没有权限）。"));
        return;
    }

    // 逐层剥开会真的解密/解压，大文件不做，免得界面卡住
    constexpr qint64 kMaxDeepScan = 64LL * 1024 * 1024;
    const bool tooBig = file.size() > kMaxDeepScan;
    QByteArray body = tooBig ? file.read(12) : file.readAll();
    file.close();

    const BackupHeaderInfo outermost = ParseLayerHeader(body);
    if (!outermost.valid) {
        restoreInfo->setStyleSheet(QStringLiteral("color:#b45309;"));
        restoreInfo->setText(QStringLiteral(
            "无法识别：不是本程序生成的备份文件，或文件已损坏。"));
        return;
    }

    // 加密层必须用密钥才能看穿，所以先在下面那组控件里试着把密钥装上
    bool keyReady = false;
    if (!restoreKeyEdit->text().trimmed().isEmpty()) {
        const auto algorithm =
            outermost.custom != 0 ? outermost.custom : static_cast<unsigned short>(1);
        QString ignored;
        keyReady = LoadKey(restoreKeySource, restoreKeyEdit, algorithm, ignored);
    }

    auto* compression = sys.Get<CompressionManager>();
    auto* encryption = sys.Get<EncryptionManager>();

    // 流水线顺序固定是 打包 -> 压缩 -> 加密，所以从外往里读到的是倒序
    QStringList chain;
    for (int depth = 0; depth < 8; ++depth) {
        const BackupHeaderInfo layer = ParseLayerHeader(body);
        if (!layer.valid) {
            if (depth > 0) {
                chain << QStringLiteral("原始文件数据");
            }
            break;
        }

        if (layer.operation == 0) {
            chain << QStringLiteral("未做处理");
            break;
        }
        if (layer.operation == 1) {
            chain << QStringLiteral("已打包");
            break; // 打包是最内层，再往里面就是各个文件了
        }
        if (layer.operation == 2) {
            chain << QStringLiteral("已压缩 (%1)")
                         .arg(AlgorithmName(Backend::CompressionAlgorithms(sys), layer.custom));
            if (tooBig) {
                chain << QStringLiteral("文件较大，未继续深入");
                break;
            }
            std::vector<std::uint8_t> plain;
            const QByteArray payload = PayloadOf(body, layer);
            if (compression == nullptr ||
                !compression->Decompression(ToVector(payload), layer.custom, plain)) {
                chain << QStringLiteral("解压失败，无法继续识别");
                break;
            }
            body = ToBytes(plain);
            continue;
        }
        if (layer.operation == 3) {
            chain << QStringLiteral("已加密 (%1)")
                         .arg(AlgorithmName(Backend::EncryptionAlgorithms(sys), layer.custom));
            if (depth == 0) {
                backupEncrypted = true;
                backupEncryptAlgorithm = layer.custom;
            }
            if (!keyReady) {
                chain << QStringLiteral("填入正确的密钥后可看到里面");
                break;
            }
            if (tooBig) {
                chain << QStringLiteral("文件较大，未继续深入");
                break;
            }
            std::vector<std::uint8_t> plain;
            const QByteArray payload = PayloadOf(body, layer);
            if (encryption == nullptr ||
                !encryption->Decryption(ToVector(payload), layer.custom, plain)) {
                chain << QStringLiteral("解密失败：密钥不对或数据损坏");
                break;
            }
            body = ToBytes(plain);
            continue;
        }

        chain << QStringLiteral("未知操作 %1").arg(layer.operation);
        break;
    }

    restoreInfo->setStyleSheet(backupEncrypted ? QStringLiteral("color:#b45309;")
                                               : QStringLiteral("color:#6b7280;"));
    restoreInfo->setText(QStringLiteral("备份内容：") + chain.join(QStringLiteral("　→　")));
}

void MainWindow::StartBackup() {
    if (busy) {
        return;
    }
    const QString src = backupSrc->text().trimmed();
    const QString dst = backupDst->text().trimmed();
    if (src.isEmpty() || dst.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"),
                             QStringLiteral("请先选择源目录和备份文件的保存位置。"));
        return;
    }

    QString error;
    if (!ApplySettingsToBackend(error)) {
        QMessageBox::warning(this, QStringLiteral("设置有问题"), error);
        return;
    }

    auto* pipeline = sys.Get<FilePipelineManager>();
    const auto srcPath = Backend::Path(src);
    const auto dstPath = Backend::Path(dst);
    RunJob(QStringLiteral("备份"),
           [pipeline, srcPath, dstPath] { return pipeline->Backup(srcPath, dstPath); },
           backupBar, backupStatus);
}

void MainWindow::StartRestore() {
    if (busy) {
        return;
    }
    const QString src = restoreSrc->text().trimmed();
    const QString dst = restoreDst->text().trimmed();
    if (src.isEmpty() || dst.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"),
                             QStringLiteral("请先选择备份文件和输出目录。"));
        return;
    }

    // 还原只需要密钥，不需要打包/压缩/加密那三个开关：
    // 备份时做了哪些处理、用了哪个算法，都记录在备份文件的头部，内核按文件里的走。
    // 唯一不在文件里的是密钥，所以要在这里单独取。
    RefreshRestoreInfo();
    const QString keyText = restoreKeyEdit->text().trimmed();
    if (backupEncrypted && keyText.isEmpty()) {
        QMessageBox::warning(
            this, QStringLiteral("需要密钥"),
            QStringLiteral("这个备份文件是加密的，请在最下面的「解密设置」里填入"
                           "与备份时相同的密钥文件或口令。\n\n"
                           "留空直接还原会因为解密失败而终止，什么也还原不出来。"));
        return;
    }
    if (!keyText.isEmpty()) {
        // 派生口令时用备份文件里记录的算法决定密钥长度（AES 32 / DES 8）
        const auto algorithm =
            backupEncryptAlgorithm != 0 ? backupEncryptAlgorithm : static_cast<unsigned short>(1);
        QString error;
        if (!LoadKey(restoreKeySource, restoreKeyEdit, algorithm, error)) {
            QMessageBox::warning(this, QStringLiteral("密钥有问题"), error);
            return;
        }
    }

    auto* pipeline = sys.Get<FilePipelineManager>();
    const auto srcPath = Backend::Path(src);
    const auto dstPath = Backend::Path(dst);
    RunJob(QStringLiteral("还原"),
           [pipeline, srcPath, dstPath] { return pipeline->Restore(srcPath, dstPath); },
           restoreBar, restoreStatus);
}

void MainWindow::RunJob(const QString& title, const std::function<bool()>& work,
                        QProgressBar* bar, QLabel* status) {
    busy = true;
    backupRun->setEnabled(false);
    restoreRun->setEnabled(false);
    bar->setVisible(true);
    status->setText(title + QStringLiteral("中…"));
    statusBar()->showMessage(title + QStringLiteral("中…"));
    AppendLog(QStringLiteral("—— %1 开始 ——").arg(title));
    Debug::Clear();

    QThread* thread = QThread::create([this, title, work, bar, status] {
        bool ok = false;
        try {
            ok = work();
        } catch (...) {
            ok = false;
        }
        const QString log = Backend::LogText();
        const bool problem = Backend::HasProblem();

        QMetaObject::invokeMethod(
            this,
            [this, title, ok, problem, log, bar, status] {
                busy = false;
                backupRun->setEnabled(true);
                restoreRun->setEnabled(true);
                bar->setVisible(false);
                AppendLog(log);

                const bool success = ok && !problem;
                AppendLog(success
                              ? QStringLiteral("—— %1 完成 ——").arg(title)
                              : QStringLiteral("—— %1 失败（见上方日志）——").arg(title));
                status->setText(title + (success ? QStringLiteral("完成") : QStringLiteral("失败")));
                statusBar()->showMessage(title +
                                         (success ? QStringLiteral("完成") : QStringLiteral("失败")));
                if (!success) {
                    tabs->setCurrentIndex(kLogTabIndex);
                    QMessageBox::warning(this, title,
                                         QStringLiteral("%1失败，详细信息见「运行日志」。").arg(title));
                }
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

// ---------------------------------------------------------------- 刷新

void MainWindow::RefreshSettingsPreview() {
    // 顺带把依赖关系同步掉：算法只在勾选时才可选
    compressAlgo->setEnabled(optCompress->isChecked());
    encryptAlgo->setEnabled(optEncrypt->isChecked());
    keySource->setEnabled(optEncrypt->isChecked());
    keyEdit->setEnabled(optEncrypt->isChecked());
    if (keyBrowse != nullptr) {
        keyBrowse->setEnabled(optEncrypt->isChecked() && keySource->currentIndex() == 0);
    }

    QStringList parts;
    parts << QStringLiteral("打包：%1")
                 .arg(optArchive->isChecked() ? QStringLiteral("开") : QStringLiteral("关"));
    parts << QStringLiteral("压缩：%1")
                 .arg(optCompress->isChecked() ? compressAlgo->currentText()
                                               : QStringLiteral("关"));
    parts << QStringLiteral("加密：%1")
                 .arg(optEncrypt->isChecked() ? encryptAlgo->currentText()
                                              : QStringLiteral("关"));
    settingsPreview->setText(QStringLiteral("当前设置　") + parts.join(QStringLiteral("　|　")));
}

void MainWindow::RefreshFilterSummary() {
    auto* filters = sys.Get<FilterManager>();
    const std::size_t count = filters != nullptr ? filters->Size() : 0;
    if (count == 0) {
        filterSummary->setText(QStringLiteral("未设置筛选条件：备份全部条目。"));
    } else {
        filterSummary->setText(
            QStringLiteral("已设置 %1 条筛选条件，全部通过才会被备份。").arg(count));
    }
}

void MainWindow::RefreshFilterTable() {
    auto* filters = sys.Get<FilterManager>();
    filterTable->setRowCount(0);
    if (filters == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < filters->Size(); ++i) {
        const Filter* filter = filters->At(i);
        if (filter == nullptr) {
            continue;
        }
        const int row = filterTable->rowCount();
        filterTable->insertRow(row);

        auto* idItem = new QTableWidgetItem(QString::number(filter->Id()));
        idItem->setData(Qt::UserRole, filter->Id());
        filterTable->setItem(row, 0, idItem);
        filterTable->setItem(row, 1, new QTableWidgetItem(Backend::Text(filter->ToString())));
        filterTable->setItem(row, 2,
                             new QTableWidgetItem(filter->IsValid()
                                                      ? QStringLiteral("有效")
                                                      : QStringLiteral("参数未识别（放行）")));
    }

    filterCount->setText(QStringLiteral("共 %1 条").arg(filters->Size()));
    RefreshFilterSummary();
}

void MainWindow::AppendLog(const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    logView->moveCursor(QTextCursor::End);
    logView->insertPlainText(text.endsWith(QLatin1Char('\n')) ? text : text + QLatin1Char('\n'));
    logView->moveCursor(QTextCursor::End);
}

// ---------------------------------------------------------------- 筛选操作

void MainWindow::AddFilter() {
    FilterDialog dialog(this, Backend::FilterTypeInfos(sys));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString spec = dialog.Spec();
    if (spec.isEmpty()) {
        return;
    }

    auto* filters = sys.Get<FilterManager>();
    const std::uint32_t id = filters->CreateAndAdd(spec.toStdString());
    if (id == 0) {
        QMessageBox::warning(this, QStringLiteral("添加失败"),
                             QStringLiteral("内核没有接受这条规格，请检查语法：") + spec);
    }
    RefreshFilterTable();
}

void MainWindow::EditSelectedFilter() {
    const int row = filterTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在列表里选中一条筛选条件。"));
        return;
    }

    const auto id = static_cast<std::uint32_t>(filterTable->item(row, 0)->data(Qt::UserRole).toUInt());
    const QString spec = filterTable->item(row, 1)->text();

    FilterDialog dialog(this, Backend::FilterTypeInfos(sys));
    dialog.setWindowTitle(QStringLiteral("编辑筛选条件"));
    dialog.SetSpec(spec);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString newSpec = dialog.Spec();
    if (newSpec.isEmpty()) {
        return;
    }

    // 内核的 set-filter 只能换参数不能换类型，所以编辑统一按「删旧 + 加新」处理
    auto* filters = sys.Get<FilterManager>();
    filters->Remove(id);
    const std::uint32_t newId = filters->CreateAndAdd(newSpec.toStdString());
    if (newId == 0) {
        QMessageBox::warning(this, QStringLiteral("修改失败"),
                             QStringLiteral("内核没有接受这条规格，请检查语法：") + newSpec);
    }
    RefreshFilterTable();
}

void MainWindow::RemoveSelectedFilter() {
    const int row = filterTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在列表里选中一条筛选条件。"));
        return;
    }

    const auto id = static_cast<std::uint32_t>(filterTable->item(row, 0)->data(Qt::UserRole).toUInt());
    if (!sys.Get<FilterManager>()->Remove(id)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"),
                             QStringLiteral("内核里找不到编号 %1 的筛选条件。").arg(id));
    }
    RefreshFilterTable();
}
