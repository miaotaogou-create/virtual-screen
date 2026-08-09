#include "SettingsPanel.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

static QSpinBox *makeSpin(QWidget *parent, int min, int max, int step, int value)
{
    auto *s = new QSpinBox(parent);
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setValue(value);
    s->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    return s;
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    setModal(true);
    setMinimumSize(540, 440);
    resize(580, 540);
    setStyleSheet(QStringLiteral(
        "QDialog { background:#0B1220; }"
        "QLabel { color:#E2E8F0; }"
        "QLineEdit, QComboBox, QSpinBox {"
        "  padding:4px; border:1px solid #334155; border-radius:2px;"
        "  background:#111827; color:#E2E8F0; selection-background-color:#0F766E;"
        "}"
        "QComboBox QAbstractItemView { background:#111827; color:#E2E8F0; selection-background-color:#0F766E; }"
        "QPushButton {"
        "  padding:6px 12px; background:#1E293B; border:1px solid #334155; color:#E2E8F0;"
        "}"
        "QPushButton:hover { background:#334155; }"
        "QPushButton#primaryBtn {"
        "  background:#0F766E; border:1px solid #0D9488; color:#fff; font-weight:600;"
        "}"
        "QPushButton#primaryBtn:hover { background:#0D9488; }"
        "QScrollArea { background:transparent; border:none; }"
        "QScrollBar:vertical { background:#0B1220; width:10px; }"
        "QScrollBar::handle:vertical { background:#334155; border-radius:4px; }"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("虚拟屏规格"), this);
    title->setStyleSheet(QStringLiteral("font-weight:600; color:#2DD4BF; font-size:13px;"));
    lay->addWidget(title);
    lay->addWidget(new QLabel(
        QStringLiteral("在此改规格并「保存方案」。真正创建虚拟屏请关对话框后点顶栏「应用」。"),
        this));

    m_driverHint = new QLabel(this);
    m_driverHint->setWordWrap(true);
    m_driverHint->setStyleSheet(QStringLiteral("color:#FCD34D;"));
    m_driverHint->hide();
    lay->addWidget(m_driverHint);

    m_profileHint = new QLabel(this);
    m_profileHint->setStyleSheet(QStringLiteral("color:#94A3B8;"));
    lay->addWidget(m_profileHint);

    auto *fileRow = new QHBoxLayout();
    fileRow->addWidget(new QLabel(QStringLiteral("配置方案"), this));
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumWidth(200);
    fileRow->addWidget(m_profileCombo, 1);
    auto *saveAsBtn = new QPushButton(QStringLiteral("另存为…"), this);
    auto *browseBtn = new QPushButton(QStringLiteral("浏览…"), this);
    fileRow->addWidget(saveAsBtn);
    fileRow->addWidget(browseBtn);
    lay->addLayout(fileRow);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollInner = new QWidget(scroll);
    scrollInner->setStyleSheet(QStringLiteral("background:transparent;"));
    m_rows = new QVBoxLayout(scrollInner);
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->addStretch();
    scroll->setWidget(scrollInner);
    lay->addWidget(scroll, 1);

    auto *editBtns = new QHBoxLayout();
    auto *addBtn = new QPushButton(QStringLiteral("添加虚拟屏"), this);
    auto *delBtn = new QPushButton(QStringLiteral("删除最后一块"), this);
    editBtns->addWidget(addBtn);
    editBtns->addWidget(delBtn);
    editBtns->addStretch();
    lay->addLayout(editBtns);

    auto *btns = new QHBoxLayout();
    auto *save = new QPushButton(QStringLiteral("保存方案"), this);
    save->setObjectName(QStringLiteral("primaryBtn"));
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    btns->addWidget(save);
    btns->addStretch();
    btns->addWidget(close);
    lay->addLayout(btns);

    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addDisplay);
    connect(delBtn, &QPushButton::clicked, this, &SettingsDialog::removeLastDisplay);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::activated),
            this, &SettingsDialog::onProfileComboChanged);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::browseLoadRequested);
    connect(saveAsBtn, &QPushButton::clicked, this, &SettingsDialog::saveAsRequested);
    connect(save, &QPushButton::clicked, this, &SettingsDialog::saveRequested);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::setDriverHint(const QString &text)
{
    if (text.isEmpty()) {
        m_driverHint->hide();
        m_driverHint->clear();
        return;
    }
    m_driverHint->setText(text);
    m_driverHint->show();
}

void SettingsDialog::setProfileHint(const QString &name)
{
    m_profileHint->setText(name.isEmpty()
                               ? QStringLiteral("当前：未命名配置")
                               : QStringLiteral("当前配置：%1").arg(name));
}

void SettingsDialog::refreshProfileList(const QString &selectName)
{
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    m_profileCombo->addItem(QStringLiteral("（选择已有配置）"), QString());
    const QStringList paths = listProfilePaths();
    int sel = 0;
    for (int i = 0; i < paths.size(); ++i) {
        const QString name = QFileInfo(paths[i]).completeBaseName();
        m_profileCombo->addItem(name, paths[i]);
        if (!selectName.isEmpty() && name == selectName)
            sel = i + 1;
    }
    m_profileCombo->setCurrentIndex(sel);
    m_profileCombo->blockSignals(false);
}

void SettingsDialog::onProfileComboChanged(int index)
{
    const QString path = m_profileCombo->itemData(index).toString();
    if (path.isEmpty())
        return;
    emit loadProfileRequested(path);
}

void SettingsDialog::rebuildRows(int count)
{
    while (m_rows->count() > 1) {
        QLayoutItem *it = m_rows->takeAt(0);
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }
    m_rowEdits.clear();
    for (int i = 0; i < count; ++i) {
        auto *box = new QWidget(this);
        box->setStyleSheet(QStringLiteral("background:transparent;"));
        auto *fl = new QFormLayout(box);
        fl->setContentsMargins(0, 0, 0, 10);
        fl->setSpacing(4);
        Row r;
        r.label = new QLineEdit(box);
        r.width = makeSpin(box, 640, 7680, 8, 1920);
        r.height = makeSpin(box, 640, 4320, 8, 1080);
        r.scale = makeSpin(box, 100, 500, 25, 100);
        r.hz = makeSpin(box, 30, 240, 1, 60);
        r.width->setMinimumWidth(90);
        r.height->setMinimumWidth(90);
        r.scale->setMinimumWidth(80);
        r.hz->setMinimumWidth(80);

        fl->addRow(QStringLiteral("屏%1 名称").arg(i + 1), r.label);

        auto *res = new QHBoxLayout();
        res->setSpacing(6);
        res->addWidget(r.width);
        res->addWidget(new QLabel(QStringLiteral("×"), box));
        res->addWidget(r.height);
        res->addStretch();
        fl->addRow(QStringLiteral("分辨率"), res);

        auto *scaleRow = new QHBoxLayout();
        scaleRow->setSpacing(6);
        scaleRow->addWidget(r.scale);
        scaleRow->addWidget(new QLabel(QStringLiteral("%"), box));
        scaleRow->addStretch();
        fl->addRow(QStringLiteral("缩放"), scaleRow);

        auto *hzRow = new QHBoxLayout();
        hzRow->setSpacing(6);
        hzRow->addWidget(r.hz);
        hzRow->addWidget(new QLabel(QStringLiteral("Hz"), box));
        hzRow->addStretch();
        fl->addRow(QStringLiteral("刷新率"), hzRow);
        m_rows->insertWidget(m_rows->count() - 1, box);
        m_rowEdits.push_back(r);
    }
}

void SettingsDialog::fillRow(int index, const DisplaySpec &s)
{
    if (index < 0 || index >= m_rowEdits.size())
        return;
    m_rowEdits[index].label->setText(s.label);
    m_rowEdits[index].width->setValue(s.width);
    m_rowEdits[index].height->setValue(s.height);
    m_rowEdits[index].scale->setValue(s.scale);
    m_rowEdits[index].hz->setValue(s.hz);
}

void SettingsDialog::loadFrom(const AppConfig &cfg)
{
    rebuildRows(qMax(1, cfg.displays.size()));
    for (int i = 0; i < cfg.displays.size() && i < m_rowEdits.size(); ++i)
        fillRow(i, cfg.displays[i]);
    setProfileHint(cfg.profileName);
    refreshProfileList(cfg.profileName);
}

AppConfig SettingsDialog::toConfig(const AppConfig &base) const
{
    AppConfig c = base;
    c.displays.clear();
    for (const Row &r : m_rowEdits) {
        DisplaySpec s;
        s.label = r.label->text().trimmed().isEmpty() ? QStringLiteral("屏") : r.label->text().trimmed();
        s.width = r.width->value();
        s.height = r.height->value();
        s.scale = r.scale->value();
        s.hz = r.hz->value();
        c.displays.push_back(s);
    }
    return c;
}

void SettingsDialog::addDisplay()
{
    if (m_rowEdits.size() >= 8) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("最多 8 块虚拟屏。"));
        return;
    }
    AppConfig cur = toConfig(AppConfig::defaults());
    DisplaySpec s;
    s.label = QStringLiteral("虚拟屏%1").arg(cur.displays.size() + 1);
    s.width = 1920;
    s.height = 1080;
    s.scale = 100;
    s.hz = 60;
    cur.displays.push_back(s);
    const QString name = m_profileHint->text().startsWith(QStringLiteral("当前配置："))
                             ? m_profileHint->text().mid(QStringLiteral("当前配置：").size())
                             : QString();
    cur.profileName = name;
    loadFrom(cur);
}

void SettingsDialog::removeLastDisplay()
{
    if (m_rowEdits.size() <= 1) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("至少保留一块虚拟屏。"));
        return;
    }
    AppConfig cur = toConfig(AppConfig::defaults());
    cur.displays.removeLast();
    const QString name = m_profileHint->text().startsWith(QStringLiteral("当前配置："))
                             ? m_profileHint->text().mid(QStringLiteral("当前配置：").size())
                             : QString();
    cur.profileName = name;
    loadFrom(cur);
}
