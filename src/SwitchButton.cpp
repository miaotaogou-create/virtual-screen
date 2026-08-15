#include "SwitchButton.h"

#include <QMouseEvent>
#include <QPainter>

SwitchButton::SwitchButton(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(40, 22);
    setCursor(Qt::PointingHandCursor);
    m_animation = new QPropertyAnimation(this, "handlePos", this);
    m_animation->setDuration(160);
}

void SwitchButton::setChecked(bool checked)
{
    if (m_checked == checked)
        return;
    m_checked = checked;
    m_animation->stop();
    m_animation->setStartValue(m_pos);
    m_animation->setEndValue(m_checked ? 1.0 : 0.0);
    m_animation->start();
    emit toggled(m_checked);
}

void SwitchButton::setHandlePos(qreal pos)
{
    m_pos = pos;
    update();
}

void SwitchButton::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    setChecked(!m_checked);
}

void SwitchButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor trackColor = m_checked ? QColor(QStringLiteral("#06b6d4"))
                                        : QColor(QStringLiteral("#334155"));
    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawRoundedRect(rect(), 11, 11);

    const qreal circleX = 3 + m_pos * (width() - 22);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(circleX + 8, 11), 8, 8);
}
