#include "PreviewPane.h"

#include <QPainter>

PreviewPane::PreviewPane(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_placeholder = QStringLiteral("暂无预览");
}

void PreviewPane::setPixmap(const QPixmap &pm)
{
    m_pm = pm;
    if (!pm.isNull())
        m_placeholder.clear();
    update();
}

void PreviewPane::setPlaceholder(const QString &text)
{
    m_placeholder = text;
    m_pm = QPixmap();
    update();
}

void PreviewPane::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0B, 0x12, 0x20));
    if (!m_pm.isNull()) {
        const QPixmap scaled = m_pm.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const int x = (width() - scaled.width()) / 2;
        const int y = (height() - scaled.height()) / 2;
        p.drawPixmap(x, y, scaled);
        return;
    }
    if (!m_placeholder.isEmpty()) {
        p.setPen(QColor(0x94, 0xA3, 0xB8));
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
    }
}
