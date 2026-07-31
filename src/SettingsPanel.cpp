#include "SettingsPanel.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    setModal(true);
    setMinimumWidth(480);
    setStyleSheet(QStringLiteral(
        "QDialog { background:#FFFFFF; }"
        "QLabel { color:#0F1C2E; }"
        "QLineEdit {"
        "  padding:4px; border:1px solid #D8E0EA; border-radius:2px;"
        "  background:#F7FAFC; color:#0F1C2E;"
        "}"
        "QPushButton { padding:6px 14px; background:#EEF2F6; border:1px solid #D8E0EA; }"
        "QPushButton:hover { background:#CCFBF1; }"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("虚拟屏规格"), this);
    title->setStyleSheet(QStringLiteral("font-weight:600; color:#0F766E; font-size:13px;"));
    lay->addWidget(title);
    lay->addWidget(new QLabel(
        QStringLiteral("物理像素 × 缩放；应用后被测程序按此逻辑分辨率运行。布局测试建议缩放 100%。"),
        this));

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

    connect(apply, &QPushButton::clicked, this, &SettingsDialog::applyRequested);
    connect(save, &QPushButton::clicked, this, &SettingsDialog::saveRequested);
    connect(clear, &QPushButton::clicked, this, &SettingsDialog::clearRequested);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::rebuildRows(int count)
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

void SettingsDialog::loadFrom(const AppConfig &cfg)
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
