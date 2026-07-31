#include "PreviewPane.h"

#include <QPainter>
#include <QResizeEvent>

PreviewPane::PreviewPane(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_placeholder = QStringLiteral("暂无预览");
}

void PreviewPane::setPixmap(const QPixmap &pm)
{
    m_source = pm;
    m_scaled = QPixmap();
    if (!pm.isNull())
        m_placeholder.clear();
    update();
}

void PreviewPane::setPlaceholder(const QString &text)
{
    m_placeholder = text;
    m_source = QPixmap();
    m_scaled = QPixmap();
    update();
}

void PreviewPane::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    m_scaled = QPixmap();
}

void PreviewPane::ensureScaled()
{
    if (m_source.isNull() || size().width() < 2 || size().height() < 2)
        return;
    if (!m_scaled.isNull())
        return;
    // Fast：预览够用；Smooth 每帧/每次 paint 会明显卡
    m_scaled = m_source.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
    if (!m_placeholder.isEmpty()) {
        p.setPen(QColor(0x94, 0xA3, 0xB8));
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
    }
}
