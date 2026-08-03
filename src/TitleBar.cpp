#include "TitleBar.h"

#include "ChromeButton.h"

#include <QFrame>
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
        "QPushButton#primaryBtn {"
        "  background:#115E59; color:#fff; font-weight:600; padding:0 12px;"
        "}"
        "QPushButton#primaryBtn:hover { background:#134E4A; }"));

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 0, 0, 0);
    lay->setSpacing(2);

    m_title = new QLabel(QStringLiteral("VirtualScreen"), this);
    m_title->setStyleSheet(QStringLiteral("font-weight:600;"));
    m_hint = new QLabel(this);
    m_hint->setStyleSheet(QStringLiteral("color:#CCFBF1; font-size:11px;"));

    auto *apply = new QPushButton(QStringLiteral("应用"), this);
    apply->setObjectName(QStringLiteral("primaryBtn"));
    apply->setToolTip(QStringLiteral("按当前配置创建/更新虚拟屏"));
    auto *clear = new QPushButton(QStringLiteral("清除"), this);
    clear->setToolTip(QStringLiteral("禁用虚拟显示驱动"));
    auto *settings = new QPushButton(QStringLiteral("设置"), this);

    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setStyleSheet(QStringLiteral("background:#99F6E4; margin:6px 4px;"));

    auto *minBtn = new ChromeButton(ChromeButton::Minimize, this);
    m_maxBtn = new ChromeButton(ChromeButton::Maximize, this);
    auto *closeBtn = new ChromeButton(ChromeButton::Close, this);
    minBtn->setToolTip(QStringLiteral("最小化"));
    m_maxBtn->setToolTip(QStringLiteral("最大化"));
    closeBtn->setToolTip(QStringLiteral("关闭"));

    lay->addWidget(m_title);
    lay->addWidget(m_hint, 1);
    lay->addWidget(apply);
    lay->addWidget(clear);
    lay->addWidget(settings);
    lay->addWidget(sep);
    lay->addWidget(minBtn);
    lay->addWidget(m_maxBtn);
    lay->addWidget(closeBtn);

    connect(apply, &QPushButton::clicked, this, &TitleBar::applyClicked);
    connect(clear, &QPushButton::clicked, this, &TitleBar::clearClicked);
    connect(settings, &QPushButton::clicked, this, &TitleBar::settingsClicked);
    connect(closeBtn, &QAbstractButton::clicked, this, &TitleBar::closeClicked);
    connect(minBtn, &QAbstractButton::clicked, this, [this]() {
        if (window())
            window()->showMinimized();
    });
    connect(m_maxBtn, &QAbstractButton::clicked, this, &TitleBar::toggleMaxRestore);
}

void TitleBar::toggleMaxRestore()
{
    QWidget *w = window();
    if (!w)
        return;
    if (w->isMaximized())
        w->showNormal();
    else
        w->showMaximized();
    syncMaxButton();
}

void TitleBar::syncMaxButton()
{
    if (!m_maxBtn || !window())
        return;
    const bool maxed = window()->isMaximized();
    m_maxBtn->setKind(maxed ? ChromeButton::Restore : ChromeButton::Maximize);
    m_maxBtn->setToolTip(maxed ? QStringLiteral("还原") : QStringLiteral("最大化"));
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
    if (e->button() == Qt::LeftButton && window() && !window()->isMaximized()) {
        m_dragging = true;
        m_dragPos = e->globalPos() - window()->frameGeometry().topLeft();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton) && window() && !window()->isMaximized())
        window()->move(e->globalPos() - m_dragPos);
    if (!(e->buttons() & Qt::LeftButton))
        m_dragging = false;
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        toggleMaxRestore();
}
