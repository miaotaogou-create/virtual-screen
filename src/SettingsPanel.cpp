#include "SettingsPanel.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    // 侧栏抽屉：必须实底，否则会透出后面的预览
    setObjectName(QStringLiteral("SettingsPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xFF, 0xFF, 0xFF));
    pal.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
    setPalette(pal);

    setStyleSheet(QStringLiteral(
        "#SettingsPanel {"
        "  background-color:#FFFFFF;"
        "  border-left:1px solid #D8E0EA;"
        "}"
        "#SettingsPanel QLabel { color:#0F1C2E; background:transparent; }"
        "#SettingsPanel QLineEdit {"
        "  padding:4px; border:1px solid #D8E0EA; border-radius:2px;"
        "  background:#F7FAFC; color:#0F1C2E;"
        "}"
        "#SettingsPanel QPushButton { padding:6px 12px; background:#EEF2F6; border:1px solid #D8E0EA; }"
        "#SettingsPanel QPushButton:hover { background:#CCFBF1; }"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("设置（侧栏）"), this);
    title->setStyleSheet(QStringLiteral("font-weight:600; color:#0F766E;"));
    lay->addWidget(title);
    lay->addWidget(new QLabel(QStringLiteral("物理像素 × 缩放；应用后被测程序按此逻辑分辨率运行。布局测试建议缩放 100%。"), this));

    m_rows = new QVBoxLayout();
    lay->addLayout(m_rows);

    auto *btns = new QHBoxLayout();
    auto *apply = new QPushButton(QStringLiteral("应用配置"), this);
    auto *save = new QPushButton(QStringLiteral("保存"), this);
    auto *clear = new QPushButton(QStringLiteral("清除虚拟屏"), this);
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    btns->addWidget(apply);
    btns->addWidget(save);
    btns->addWidget(clear);
    btns->addStretch();
    btns->addWidget(close);
    lay->addLayout(btns);
    lay->addStretch();

    connect(apply, &QPushButton::clicked, this, &SettingsPanel::applyRequested);
    connect(save, &QPushButton::clicked, this, &SettingsPanel::saveRequested);
    connect(clear, &QPushButton::clicked, this, &SettingsPanel::clearRequested);
    connect(close, &QPushButton::clicked, this, &SettingsPanel::closeRequested);
}

void SettingsPanel::rebuildRows(int count)
{
    while (m_rows->count() > 0) {
        QLayoutItem *it = m_rows->takeAt(0);
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }
    m_rowEdits.clear();
    for (int i = 0; i < count; ++i) {
        auto *box = new QWidget(this);
        auto *fl = new QFormLayout(box);
        fl->setContentsMargins(0, 0, 0, 8);
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
        m_rows->addWidget(box);
        m_rowEdits.push_back(r);
    }
}

void SettingsPanel::loadFrom(const AppConfig &cfg)
{
    rebuildRows(cfg.displays.size());
    for (int i = 0; i < cfg.displays.size(); ++i) {
        const DisplaySpec &s = cfg.displays[i];
        m_rowEdits[i].label->setText(s.label);
        m_rowEdits[i].width->setText(QString::number(s.width));
        m_rowEdits[i].height->setText(QString::number(s.height));
        m_rowEdits[i].scale->setText(QString::number(s.scale));
        m_rowEdits[i].hz->setText(QString::number(s.hz));
    }
}

AppConfig SettingsPanel::toConfig(const AppConfig &base) const
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
