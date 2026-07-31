#include "SettingsPanel.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    setModal(true);
    setMinimumSize(520, 420);
    resize(560, 520);
    setStyleSheet(QStringLiteral(
        "QDialog { background:#FFFFFF; }"
        "QLabel { color:#0F1C2E; }"
        "QLineEdit {"
        "  padding:4px; border:1px solid #D8E0EA; border-radius:2px;"
        "  background:#F7FAFC; color:#0F1C2E;"
        "}"
        "QPushButton { padding:6px 12px; background:#EEF2F6; border:1px solid #D8E0EA; }"
        "QPushButton:hover { background:#CCFBF1; }"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("虚拟屏规格"), this);
    title->setStyleSheet(QStringLiteral("font-weight:600; color:#0F766E; font-size:13px;"));
    lay->addWidget(title);
    lay->addWidget(new QLabel(
        QStringLiteral("按项目增减虚拟屏；可另存/加载配置文件。布局测试建议缩放 100%。"),
        this));

    m_profileHint = new QLabel(this);
    m_profileHint->setStyleSheet(QStringLiteral("color:#5B6B7C;"));
    lay->addWidget(m_profileHint);

    auto *fileBtns = new QHBoxLayout();
    auto *loadBtn = new QPushButton(QStringLiteral("加载配置…"), this);
    auto *saveAsBtn = new QPushButton(QStringLiteral("另存为…"), this);
    fileBtns->addWidget(loadBtn);
    fileBtns->addWidget(saveAsBtn);
    fileBtns->addStretch();
    lay->addLayout(fileBtns);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollInner = new QWidget(scroll);
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
    auto *apply = new QPushButton(QStringLiteral("应用配置"), this);
    auto *save = new QPushButton(QStringLiteral("保存为当前"), this);
    auto *clear = new QPushButton(QStringLiteral("清除虚拟屏"), this);
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    btns->addWidget(apply);
    btns->addWidget(save);
    btns->addWidget(clear);
    btns->addStretch();
    btns->addWidget(close);
    lay->addLayout(btns);

    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addDisplay);
    connect(delBtn, &QPushButton::clicked, this, &SettingsDialog::removeLastDisplay);
    connect(loadBtn, &QPushButton::clicked, this, &SettingsDialog::loadRequested);
    connect(saveAsBtn, &QPushButton::clicked, this, &SettingsDialog::saveAsRequested);
    connect(apply, &QPushButton::clicked, this, &SettingsDialog::applyRequested);
    connect(save, &QPushButton::clicked, this, &SettingsDialog::saveRequested);
    connect(clear, &QPushButton::clicked, this, &SettingsDialog::clearRequested);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::setProfileHint(const QString &name)
{
    m_profileHint->setText(name.isEmpty()
                               ? QStringLiteral("当前：未命名配置")
                               : QStringLiteral("当前配置：%1").arg(name));
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
        auto *fl = new QFormLayout(box);
        fl->setContentsMargins(0, 0, 0, 10);
        fl->setSpacing(4);
        Row r;
        r.label = new QLineEdit(box);
        r.width = new QLineEdit(box);
        r.height = new QLineEdit(box);
        r.scale = new QLineEdit(box);
        r.hz = new QLineEdit(box);
        fl->addRow(QStringLiteral("屏%1 名称").arg(i + 1), r.label);
        auto *res = new QHBoxLayout();
        res->addWidget(r.width);
        res->addWidget(new QLabel(QStringLiteral("×"), box));
        res->addWidget(r.height);
        res->addWidget(new QLabel(QStringLiteral("缩放%"), box));
        res->addWidget(r.scale);
        res->addWidget(new QLabel(QStringLiteral("Hz"), box));
        res->addWidget(r.hz);
        fl->addRow(QStringLiteral("分辨率"), res);
        m_rows->insertWidget(m_rows->count() - 1, box);
        m_rowEdits.push_back(r);
    }
}

void SettingsDialog::fillRow(int index, const DisplaySpec &s)
{
    if (index < 0 || index >= m_rowEdits.size())
        return;
    m_rowEdits[index].label->setText(s.label);
    m_rowEdits[index].width->setText(QString::number(s.width));
    m_rowEdits[index].height->setText(QString::number(s.height));
    m_rowEdits[index].scale->setText(QString::number(s.scale));
    m_rowEdits[index].hz->setText(QString::number(s.hz));
}

void SettingsDialog::loadFrom(const AppConfig &cfg)
{
    rebuildRows(qMax(1, cfg.displays.size()));
    for (int i = 0; i < cfg.displays.size() && i < m_rowEdits.size(); ++i)
        fillRow(i, cfg.displays[i]);
    setProfileHint(cfg.profileName);
}

AppConfig SettingsDialog::toConfig(const AppConfig &base) const
{
    AppConfig c = base;
    c.displays.clear();
    for (const Row &r : m_rowEdits) {
        DisplaySpec s;
        s.label = r.label->text().trimmed().isEmpty() ? QStringLiteral("屏") : r.label->text().trimmed();
        s.width = r.width->text().toInt();
        s.height = r.height->text().toInt();
        s.scale = r.scale->text().toInt();
        s.hz = r.hz->text().toInt();
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
    cur.profileName.clear();
    DisplaySpec s;
    s.label = QStringLiteral("虚拟屏%1").arg(cur.displays.size() + 1);
    s.width = 1920;
    s.height = 1080;
    s.scale = 100;
    s.hz = 60;
    cur.displays.push_back(s);
    // 保留原提示名
    const QString hint = m_profileHint->text();
    loadFrom(cur);
    if (hint.startsWith(QStringLiteral("当前配置：")))
        setProfileHint(hint.mid(QStringLiteral("当前配置：").size()));
}

void SettingsDialog::removeLastDisplay()
{
    if (m_rowEdits.size() <= 1) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("至少保留一块虚拟屏。"));
        return;
    }
    AppConfig cur = toConfig(AppConfig::defaults());
    cur.displays.removeLast();
    const QString hint = m_profileHint->text();
    loadFrom(cur);
    if (hint.startsWith(QStringLiteral("当前配置：")))
        setProfileHint(hint.mid(QStringLiteral("当前配置：").size()));
}
