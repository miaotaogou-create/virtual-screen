#include "TitleBar.h"

#include "ChromeButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSvgRenderer>
#include <QStyle>

namespace {

/** 标题栏主图标：加载 Fluent SVG 矢量资源。 */
class AppIcon : public QWidget
{
public:
    explicit AppIcon(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("AppIcon"));
        setFixedSize(28, 28);
        m_renderer.load(QStringLiteral(":/app_icon.svg"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        m_renderer.render(&p, rect());
    }

private:
    QSvgRenderer m_renderer;
};

} // namespace

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("TitleBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(46);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(14, 6, 6, 6);
    lay->setSpacing(10);

    lay->addWidget(new AppIcon(this));

    auto *appTitle = new QLabel(QStringLiteral("虚拟屏助手"), this);
    appTitle->setObjectName(QStringLiteral("AppTitle"));

    auto *ver = new QLabel(QStringLiteral("v1.1 Fluent"), this);
    ver->setObjectName(QStringLiteral("VersionBadge"));
    ver->setFixedHeight(22);
    ver->setAlignment(Qt::AlignCenter);

    m_statusPill = new QWidget(this);
    m_statusPill->setObjectName(QStringLiteral("DriverStatusPill"));
    m_statusPill->setProperty("ready", false);
    m_statusPill->setFixedHeight(22);
    auto *pillLay = new QHBoxLayout(m_statusPill);
    pillLay->setContentsMargins(8, 0, 10, 0);
    pillLay->setSpacing(6);
    m_statusDot = new QLabel(m_statusPill);
    m_statusDot->setObjectName(QStringLiteral("StatusDot"));
    m_statusDot->setFixedSize(6, 6);
    m_statusText = new QLabel(QStringLiteral("驱动检测中…"), m_statusPill);
    m_statusText->setObjectName(QStringLiteral("StatusText"));
    pillLay->addWidget(m_statusDot, 0, Qt::AlignVCenter);
    pillLay->addWidget(m_statusText, 0, Qt::AlignVCenter);

    lay->addWidget(appTitle);
    lay->addWidget(ver);
    lay->addWidget(m_statusPill);
    lay->addStretch();

    auto *minBtn = new ChromeButton(ChromeButton::Minimize, this);
    m_maxBtn = new ChromeButton(ChromeButton::Maximize, this);
    auto *closeBtn = new ChromeButton(ChromeButton::Close, this);
    minBtn->setFixedSize(40, 32);
    m_maxBtn->setFixedSize(40, 32);
    closeBtn->setFixedSize(40, 32);

    lay->addWidget(minBtn);
    lay->addWidget(m_maxBtn);
    lay->addWidget(closeBtn);

    connect(closeBtn, &QAbstractButton::clicked, this, &TitleBar::closeClicked);
    connect(minBtn, &QAbstractButton::clicked, this, [this]() {
        if (window())
            window()->showMinimized();
    });
    connect(m_maxBtn, &QAbstractButton::clicked, this, &TitleBar::toggleMaxRestore);
}

void TitleBar::setBusy(bool busy)
{
    Q_UNUSED(busy);
}

void TitleBar::setDriverReady(bool ready, int virtualCount)
{
    m_statusPill->setProperty("ready", ready);
    if (ready) {
        m_statusText->setText(
            QStringLiteral("Parsec 驱动就绪 (%1 屏运行)").arg(qMax(virtualCount, 0)));
    } else {
        m_statusText->setText(QStringLiteral("未检测到 Parsec 驱动"));
    }
    m_statusPill->style()->unpolish(m_statusPill);
    m_statusPill->style()->polish(m_statusPill);
    m_statusDot->style()->unpolish(m_statusDot);
    m_statusDot->style()->polish(m_statusDot);
    m_statusText->style()->unpolish(m_statusText);
    m_statusText->style()->polish(m_statusText);
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
}

void TitleBar::setStatusHint(const QString &text)
{
    if (!text.isEmpty())
        m_statusPill->setToolTip(text);
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
