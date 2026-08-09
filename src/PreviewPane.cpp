#include "PreviewPane.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

PreviewPane::PreviewPane(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_placeholder = QStringLiteral("暂无预览");

    m_guide = new QWidget(this);
    m_guide->setStyleSheet(QStringLiteral(
        "QWidget#guideCard {"
        "  background:#111827; border:1px solid #334155;"
        "}"
        "QLabel#guideTitle { color:#2DD4BF; font-size:15px; font-weight:600; background:transparent; }"
        "QLabel#guideBody { color:#CBD5E1; font-size:13px; background:transparent; }"
        "QPushButton {"
        "  padding:8px 18px; background:#1E293B; border:1px solid #334155; color:#E2E8F0;"
        "}"
        "QPushButton:hover { background:#334155; }"
        "QPushButton#primaryBtn {"
        "  background:#0F766E; border:1px solid #0D9488; color:#fff; font-weight:600;"
        "}"
        "QPushButton#primaryBtn:hover { background:#0D9488; }"
        "QPushButton:disabled { color:#64748B; background:#0F172A; }"));
    m_guide->setObjectName(QStringLiteral("guideCard"));

    auto *lay = new QVBoxLayout(m_guide);
    lay->setContentsMargins(28, 24, 28, 24);
    lay->setSpacing(14);
    m_guideTitle = new QLabel(m_guide);
    m_guideTitle->setObjectName(QStringLiteral("guideTitle"));
    m_guideTitle->setWordWrap(true);
    m_guideBody = new QLabel(m_guide);
    m_guideBody->setObjectName(QStringLiteral("guideBody"));
    m_guideBody->setWordWrap(true);
    m_guideBody->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    auto *btns = new QHBoxLayout();
    btns->setSpacing(10);
    m_primary = new QPushButton(m_guide);
    m_primary->setObjectName(QStringLiteral("primaryBtn"));
    m_primary->setCursor(Qt::PointingHandCursor);
    m_secondary = new QPushButton(m_guide);
    m_secondary->setCursor(Qt::PointingHandCursor);
    btns->addWidget(m_primary);
    btns->addWidget(m_secondary);
    btns->addStretch();
    lay->addWidget(m_guideTitle);
    lay->addWidget(m_guideBody);
    lay->addLayout(btns);
    m_guide->hide();

    connect(m_primary, &QPushButton::clicked, this, &PreviewPane::primaryClicked);
    connect(m_secondary, &QPushButton::clicked, this, &PreviewPane::secondaryClicked);
}

void PreviewPane::showGuidePanel(bool on)
{
    m_guide->setVisible(on);
    if (on)
        layoutGuide();
}

void PreviewPane::layoutGuide()
{
    if (!m_guide->isVisible())
        return;
    m_guide->adjustSize();
    const int w = qMin(520, qMax(360, width() - 80));
    m_guide->setFixedWidth(w);
    m_guide->adjustSize();
    const int x = (width() - m_guide->width()) / 2;
    const int y = (height() - m_guide->height()) / 2;
    m_guide->move(qMax(0, x), qMax(0, y));
}

void PreviewPane::setPixmap(const QPixmap &pm)
{
    m_source = pm;
    m_scaled = QPixmap();
    if (!pm.isNull()) {
        m_placeholder.clear();
        showGuidePanel(false);
    }
    update();
}

void PreviewPane::setPlaceholder(const QString &text)
{
    m_placeholder = text;
    m_source = QPixmap();
    m_scaled = QPixmap();
    showGuidePanel(false);
    update();
}

void PreviewPane::setGuide(const QString &title, const QString &body,
                           const QString &primaryText, const QString &secondaryText)
{
    m_source = QPixmap();
    m_scaled = QPixmap();
    m_placeholder.clear();
    m_guideTitle->setText(title);
    m_guideBody->setText(body);
    m_primary->setText(primaryText);
    m_primary->setVisible(!primaryText.isEmpty());
    m_secondary->setText(secondaryText);
    m_secondary->setVisible(!secondaryText.isEmpty());
    showGuidePanel(true);
    update();
}

void PreviewPane::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    m_scaled = QPixmap();
    layoutGuide();
}

void PreviewPane::ensureScaled()
{
    if (m_source.isNull() || size().width() < 2 || size().height() < 2)
        return;
    if (!m_scaled.isNull())
        return;
    const QSize fitted = m_source.size().scaled(size(), Qt::KeepAspectRatio);
    if (m_source.size() == fitted) {
        m_scaled = m_source;
        return;
    }
    m_scaled = m_source.scaled(fitted, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void PreviewPane::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0B, 0x12, 0x20));
    if (!m_source.isNull()) {
        ensureScaled();
        if (!m_scaled.isNull()) {
            const int x = (width() - m_scaled.width()) / 2;
            const int y = (height() - m_scaled.height()) / 2;
            p.drawPixmap(x, y, m_scaled);
        }
        return;
    }
    if (m_guide->isVisible())
        return;
    if (!m_placeholder.isEmpty()) {
        p.setPen(QColor(0x94, 0xA3, 0xB8));
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
    }
}
