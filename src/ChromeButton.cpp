#include "ChromeButton.h"

#include <QPainter>
#include <QPainterPath>

ChromeButton::ChromeButton(Kind kind, QWidget *parent)
    : QAbstractButton(parent)
    , m_kind(kind)
{
    setFixedSize(36, 28);
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);
}

void ChromeButton::setKind(Kind kind)
{
    m_kind = kind;
    update();
}

void ChromeButton::enterEvent(QEvent *)
{
    m_hover = true;
    update();
}

void ChromeButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void ChromeButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 悬停底
    if (m_hover) {
        p.fillRect(rect(), m_kind == Close ? QColor(0xB9, 0x1C, 0x1C) : QColor(0x0D, 0x94, 0x88));
    }

    // 统一画在 10×10 的几何中心区域，线宽一致
    const qreal cx = width() * 0.5;
    const qreal cy = height() * 0.5;
    const qreal s = 5.0; // 半宽

    QPen pen(QColor(0xEC, 0xFD, 0xF5));
    pen.setWidthF(1.4);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (m_kind) {
    case Minimize:
        p.drawLine(QPointF(cx - s, cy), QPointF(cx + s, cy));
        break;
    case Maximize:
        p.drawRect(QRectF(cx - s, cy - s, s * 2, s * 2));
        break;
    case Restore: {
        const QColor bg = m_hover ? QColor(0x0D, 0x94, 0x88) : QColor(0x0F, 0x76, 0x6E);
        // 后框
        p.drawRect(QRectF(cx - s + 2.0, cy - s, s * 2 - 2.0, s * 2 - 2.0));
        // 前框（盖住一部分）
        p.fillRect(QRectF(cx - s, cy - s + 2.0, s * 2 - 2.0, s * 2 - 2.0), bg);
        p.drawRect(QRectF(cx - s, cy - s + 2.0, s * 2 - 2.0, s * 2 - 2.0));
        break;
    }
    case Close:
        p.drawLine(QPointF(cx - s, cy - s), QPointF(cx + s, cy + s));
        p.drawLine(QPointF(cx + s, cy - s), QPointF(cx - s, cy + s));
        break;
    }
}
