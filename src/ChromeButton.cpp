#include "ChromeButton.h"

#include <QMouseEvent>
#include <QPainter>

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
    m_pressed = false;
    update();
}

void ChromeButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
    QAbstractButton::mousePressEvent(e);
}

void ChromeButton::mouseReleaseEvent(QMouseEvent *e)
{
    m_pressed = false;
    update();
    QAbstractButton::mouseReleaseEvent(e);
}

void ChromeButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_pressed) {
        p.fillRect(rect(), m_kind == Close ? QColor(0x7F, 0x1D, 0x1D) : QColor(0x11, 0x5E, 0x59));
    } else if (m_hover) {
        p.fillRect(rect(), m_kind == Close ? QColor(0xB9, 0x1C, 0x1C) : QColor(0x0D, 0x94, 0x88));
    }

    const qreal cx = width() * 0.5;
    const qreal cy = height() * 0.5;
    const qreal s = 5.0;

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
        const QColor bg = m_pressed ? QColor(0x11, 0x5E, 0x59)
                                    : (m_hover ? QColor(0x0D, 0x94, 0x88) : QColor(0x0F, 0x76, 0x6E));
        p.drawRect(QRectF(cx - s + 2.0, cy - s, s * 2 - 2.0, s * 2 - 2.0));
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
