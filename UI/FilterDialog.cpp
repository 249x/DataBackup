#include "FilterDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <utility>

namespace {

// 与 FilterManager::Initialize() 的注册顺序严格一致
// （FilterManager::TypeInfos() 就是按注册顺序返回的，别改成按名字排序）
enum TypeIndex {
    TypeName = 0,
    TypePath,
    TypeOwner,
    TypeFileType,
    TypeSize,
    TypeTime,
    TypeAnd,
    TypeOr,
    TypeNot,
    TypeCount
};

// 名字必须和 FileType.h 里的 FileTypeName() 完全一致：TypeFilter 是用
// FileTypeFromName() 做精确匹配的，大小写和拼写都不能改。
struct FileTypeChoice {
    const char* name;
    const char* note;
};

const FileTypeChoice kFileTypes[] = {
    {"regular", "普通文件"},      {"directory", "目录"},
    {"symlink", "符号链接"},      {"junction", "目录联接"},
    {"hardlink", "硬链接"},       {"fifo", "命名管道"},
    {"socket", "套接字"},         {"block_device", "块设备"},
    {"character_device", "字符设备"}, {"other", "其它 / 未识别"},
};

} // namespace

FilterDialog::FilterDialog(QWidget* parent, std::vector<FilterTypeInfo> types)
    : QDialog(parent), typeInfos(std::move(types)) {
    setWindowTitle(QStringLiteral("添加筛选条件"));
    setMinimumWidth(560);
    BuildUi();
    Rebuild();
}

int FilterDialog::PageOfType(int typeIndex) const {
    // 前 6 种各有一页；and / or / not 共用最后一页（子条件文本域）
    if (typeIndex <= TypeTime) {
        return typeIndex;
    }
    return TypeTime + 1;
}

void FilterDialog::BuildUi() {
    auto* root = new QVBoxLayout(this);

    auto* top = new QFormLayout();
    typeBox = new QComboBox(this);
    for (const FilterTypeInfo& info : typeInfos) {
        typeBox->addItem(QString::fromUtf8(info.name));
    }
    top->addRow(QStringLiteral("筛选类型"), typeBox);
    hintLabel = new QLabel(this);
    hintLabel->setWordWrap(true);
    top->addRow(QString(), hintLabel);
    root->addLayout(top);

    pages = new QStackedWidget(this);

    // ---- 0. name / 1. path / 2. owner：都是一个通配模式 ----
    auto makePatternPage = [this](const char* caption, const char* placeholder, QLineEdit** slot) {
        auto* page = new QWidget(this);
        auto* form = new QFormLayout(page);
        form->setContentsMargins(0, 0, 0, 0);
        auto* edit = new QLineEdit(page);
        edit->setPlaceholderText(QString::fromUtf8(placeholder));
        form->addRow(QString::fromUtf8(caption), edit);
        *slot = edit;
        connect(edit, &QLineEdit::textChanged, this, [this] { Rebuild(); });
        return page;
    };

    pages->addWidget(makePatternPage("文件名模式", "*.tmp", &nameEdit));
    pages->addWidget(makePatternPage("相对路径模式", "src/*.csv", &pathEdit));
    pages->addWidget(makePatternPage("属主模式", "*admin*", &ownerEdit));

    // ---- 3. type：10 个类型名复选 ----
    {
        auto* page = new QWidget(this);
        auto* grid = new QGridLayout(page);
        grid->setContentsMargins(0, 0, 0, 0);
        int index = 0;
        for (const FileTypeChoice& choice : kFileTypes) {
            auto* box = new QCheckBox(QString::fromUtf8(choice.name), page);
            box->setToolTip(QString::fromUtf8(choice.note));
            connect(box, &QCheckBox::toggled, this, [this] { Rebuild(); });
            typeChecks.push_back(box);
            grid->addWidget(box, index / 3, index % 3);
            ++index;
        }
        pages->addWidget(page);
    }

    // ---- 4. size ----
    {
        auto* page = new QWidget(this);
        auto* form = new QFormLayout(page);
        form->setContentsMargins(0, 0, 0, 0);
        sizeOp = new QComboBox(page);
        sizeOp->addItems({QStringLiteral("不小于 (min=)"), QStringLiteral("不大于 (max=)"),
                          QStringLiteral("恰好等于 (=)"), QStringLiteral("区间 (A..B)")});
        sizeA = new QLineEdit(page);
        sizeA->setPlaceholderText(QStringLiteral("例如 1M、512k、0"));
        sizeB = new QLineEdit(page);
        sizeB->setPlaceholderText(QStringLiteral("区间上界，例如 2M"));
        form->addRow(QStringLiteral("条件"), sizeOp);
        form->addRow(QStringLiteral("值"), sizeA);
        form->addRow(QStringLiteral("上界"), sizeB);
        connect(sizeOp, &QComboBox::currentIndexChanged, this, [this](int i) {
            sizeB->setEnabled(i == 3);
            Rebuild();
        });
        connect(sizeA, &QLineEdit::textChanged, this, [this] { Rebuild(); });
        connect(sizeB, &QLineEdit::textChanged, this, [this] { Rebuild(); });
        sizeB->setEnabled(false);
        pages->addWidget(page);
    }

    // ---- 5. time ----
    {
        auto* page = new QWidget(this);
        auto* form = new QFormLayout(page);
        form->setContentsMargins(0, 0, 0, 0);
        timeField = new QComboBox(page);
        timeField->addItems({QStringLiteral("修改时间"), QStringLiteral("访问时间"),
                             QStringLiteral("创建时间")});
        timeOp = new QComboBox(page);
        timeOp->addItems({QStringLiteral("不早于 (min=)"), QStringLiteral("不晚于 (max=)"),
                          QStringLiteral("区间 (A..B)")});
        timeA = new QLineEdit(page);
        timeA->setPlaceholderText(QStringLiteral("YYYY-MM-DD"));
        timeB = new QLineEdit(page);
        timeB->setPlaceholderText(QStringLiteral("YYYY-MM-DD"));
        form->addRow(QStringLiteral("时间字段"), timeField);
        form->addRow(QStringLiteral("条件"), timeOp);
        form->addRow(QStringLiteral("起"), timeA);
        form->addRow(QStringLiteral("止"), timeB);
        connect(timeField, &QComboBox::currentIndexChanged, this, [this] { Rebuild(); });
        connect(timeOp, &QComboBox::currentIndexChanged, this, [this](int i) {
            timeB->setEnabled(i == 2);
            Rebuild();
        });
        connect(timeA, &QLineEdit::textChanged, this, [this] { Rebuild(); });
        connect(timeB, &QLineEdit::textChanged, this, [this] { Rebuild(); });
        timeB->setEnabled(false);
        pages->addWidget(page);
    }

    // ---- 6. and / or / not：子条件，一行一个 ----
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(new QLabel(QStringLiteral("子条件，一行写一个，内部会自动用 ; 连接："), page));
        childText = new QPlainTextEdit(page);
        childText->setPlaceholderText(QStringLiteral("name:*.tmp\nsize:max=1M"));
        childText->setFixedHeight(96);
        layout->addWidget(childText);
        connect(childText, &QPlainTextEdit::textChanged, this, [this] { Rebuild(); });
        pages->addWidget(page);
    }

    root->addWidget(pages);

    auto* previewBox = new QGroupBox(QStringLiteral("将提交的规格（可直接修改）"), this);
    auto* previewLayout = new QVBoxLayout(previewBox);
    specText = new QPlainTextEdit(previewBox);
    specText->setFixedHeight(60);
    previewLayout->addWidget(specText);
    root->addWidget(previewBox);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(typeBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        pages->setCurrentIndex(PageOfType(index));
        if (index >= 0 && index < static_cast<int>(typeInfos.size())) {
            hintLabel->setText(QStringLiteral("语法 %1　例：%2　—　%3")
                                   .arg(QString::fromUtf8(typeInfos[index].syntax),
                                        QString::fromUtf8(typeInfos[index].example),
                                        QString::fromUtf8(typeInfos[index].hint)));
        }
        Rebuild();
    });
    typeBox->setCurrentIndex(TypeName);
    pages->setCurrentIndex(PageOfType(TypeName));
    hintLabel->setText(QStringLiteral("语法 %1　例：%2　—　%3")
                           .arg(QString::fromUtf8(typeInfos[TypeName].syntax),
                                QString::fromUtf8(typeInfos[TypeName].example),
                                QString::fromUtf8(typeInfos[TypeName].hint)));
}

QString FilterDialog::Compose() const {
    const int index = typeBox->currentIndex();
    if (index < 0 || index >= static_cast<int>(typeInfos.size())) {
        return QString();
    }
    const QString type = QString::fromUtf8(typeInfos[index].name);

    switch (index) {
    case TypeName:
        return QStringLiteral("name:") + nameEdit->text().trimmed();
    case TypePath:
        return QStringLiteral("path:") + pathEdit->text().trimmed();
    case TypeOwner:
        return QStringLiteral("owner:") + ownerEdit->text().trimmed();
    case TypeFileType: {
        QStringList picked;
        for (const QCheckBox* box : typeChecks) {
            if (box->isChecked()) {
                picked << box->text();
            }
        }
        return QStringLiteral("type:") + picked.join(QLatin1Char(','));
    }
    case TypeSize: {
        const QString low = sizeA->text().trimmed();
        const QString high = sizeB->text().trimmed();
        switch (sizeOp->currentIndex()) {
        case 0:  return QStringLiteral("size:min=") + low;
        case 1:  return QStringLiteral("size:max=") + low;
        case 2:  return QStringLiteral("size:=") + low;
        default: return QStringLiteral("size:") + low + QStringLiteral("..") + high;
        }
    }
    case TypeTime: {
        QString cond;
        const QString low = timeA->text().trimmed();
        const QString high = timeB->text().trimmed();
        switch (timeOp->currentIndex()) {
        case 0:  cond = QStringLiteral("min=") + low; break;
        case 1:  cond = QStringLiteral("max=") + low; break;
        default: cond = low + QStringLiteral("..") + high; break;
        }
        // 默认字段是 write，只有 access / create 才需要显式写出来
        // （关键词由 TimeFilter::ParseField 按小写精确匹配）
        static const char* kFields[] = {"", "access ", "create "};
        const int fieldIndex = timeField->currentIndex();
        const QString field = (fieldIndex >= 0 && fieldIndex < 3)
                                  ? QString::fromLatin1(kFields[fieldIndex])
                                  : QString();
        return QStringLiteral("time:") + field + cond;
    }
    default: {
        QStringList children;
        const QStringList lines = childText->toPlainText().split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                children << trimmed;
            }
        }
        return type + QStringLiteral(":(") + children.join(QLatin1Char(';')) + QStringLiteral(")");
    }
    }
}

void FilterDialog::Rebuild() {
    if (specText == nullptr) {
        return;
    }
    specText->setPlainText(Compose());
}

QString FilterDialog::Spec() const {
    return specText->toPlainText().trimmed();
}

void FilterDialog::SetSpec(const QString& spec) {
    specText->setPlainText(spec);
}
