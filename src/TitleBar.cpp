#include "TitleBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(28);
    setStyleSheet(QStringLiteral(
        "TitleBar { background:#0F766E; }"
        "QLabel { color:#fff; background:transparent; }"
        "QPushButton { background:transparent; color:#ECFDF5; border:none; padding:0 10px; }"
        "QPushButton:hover { background:#0D9488; }"
        "QPushButton#closeBtn:hover { background:#B91C1C; }"));

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 0, 4, 0);
    lay->setSpacing(4);

    m_title = new QLabel(QStringLiteral("VirtualScreen"), this);
    m_title->setStyleSheet(QStringLiteral("font-weight:600;"));
    m_hint = new QLabel(this);
    m_hint->setStyleSheet(QStringLiteral("color:#CCFBF1; font-size:11px;"));

    auto *apply = new QPushButton(QStringLiteral("应用"), this);
    auto *clear = new QPushButton(QStringLiteral("清除"), this);
    auto *settings = new QPushButton(QStringLiteral("设置"), this);
    auto *minBtn = new QPushButton(QStringLiteral("—"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setObjectName(QStringLiteral("closeBtn"));
    minBtn->setFixedWidth(28);
    closeBtn->setFixedWidth(28);

    lay->addWidget(m_title);
    lay->addWidget(m_hint, 1);
    lay->addWidget(apply);
    lay->addWidget(clear);
    lay->addWidget(settings);
    lay->addWidget(minBtn);
    lay->addWidget(closeBtn);

    connect(apply, &QPushButton::clicked, this, &TitleBar::applyClicked);
    connect(clear, &QPushButton::clicked, this, &TitleBar::clearClicked);
    connect(settings, &QPushButton::clicked, this, &TitleBar::settingsClicked);
    connect(closeBtn, &QPushButton::clicked, this, &TitleBar::closeClicked);
    connect(minBtn, &QPushButton::clicked, this, [this]() {
        if (window())
            window()->showMinimized();
    });
}

void TitleBar::setAdminHint(const QString &text)
{
    m_title->setText(QStringLiteral("VirtualScreen · ") + text);
}

void TitleBar::setStatusHint(const QString &text)
{
    m_hint->setText(text);
}

void TitleBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPos = e->globalPos() - window()->frameGeometry().topLeft();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton) && window())
        window()->move(e->globalPos() - m_dragPos);
    if (!(e->buttons() & Qt::LeftButton))
        m_dragging = false;
}
