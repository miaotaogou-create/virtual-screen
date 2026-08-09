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
        "QPushButton:disabled { color:#99F6E4; }"));

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 0, 0, 0);
    lay->setSpacing(2);

    m_hint = new QLabel(this);
    m_hint->setStyleSheet(QStringLiteral("color:#CCFBF1; font-size:11px;"));

    m_clear = new QPushButton(QStringLiteral("清除全部"), this);
    m_clear->setToolTip(QStringLiteral("移除所有虚拟屏"));

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

    lay->addWidget(m_hint, 1);
    lay->addWidget(m_clear);
    lay->addWidget(sep);
    lay->addWidget(minBtn);
    lay->addWidget(m_maxBtn);
    lay->addWidget(closeBtn);

    connect(m_clear, &QPushButton::clicked, this, &TitleBar::clearClicked);
    connect(closeBtn, &QAbstractButton::clicked, this, &TitleBar::closeClicked);
    connect(minBtn, &QAbstractButton::clicked, this, [this]() {
        if (window())
            window()->showMinimized();
    });
    connect(m_maxBtn, &QAbstractButton::clicked, this, &TitleBar::toggleMaxRestore);
}

void TitleBar::setBusy(bool busy)
{
    m_clear->setEnabled(!busy);
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
