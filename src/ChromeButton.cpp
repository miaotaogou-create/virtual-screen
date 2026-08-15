#include "ChromeButton.h"

#include <QMouseEvent>
#include <QPainter>

namespace {

const QColor kIconNormal(0x94, 0xa3, 0xb8);
const QColor kIconHover(0xff, 0xff, 0xff);
const QColor kTitleBarBg(0x07, 0x0e, 0x18);
const QColor kHoverChromeBg(0xff, 0xff, 0xff, 20); // rgba(255,255,255,0.08)
const QColor kHoverCloseBg(0xe1, 0x1d, 0x48);
const QColor kPressedCloseBg(0xbe, 0x12, 0x3c);
const QColor kPressedChromeBg(0xff, 0xff, 0xff, 30);

} // namespace

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

    if (m_kind == Close) {
        if (m_pressed)
            p.fillRect(rect(), kPressedCloseBg);
        else if (m_hover)
            p.fillRect(rect(), kHoverCloseBg);
    } else if (m_pressed) {
        p.fillRect(rect(), kPressedChromeBg);
    } else if (m_hover) {
        p.fillRect(rect(), kHoverChromeBg);
    }

    const qreal cx = width() * 0.5;
    const qreal cy = height() * 0.5;
    const qreal s = 4.5;

    const QColor iconColor = (m_hover || m_pressed) ? kIconHover : kIconNormal;
    QPen pen(iconColor);
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
        const QColor bg = m_pressed ? kPressedChromeBg
                                    : (m_hover ? kHoverChromeBg : kTitleBarBg);
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
