#pragma once

#include <QPropertyAnimation>
#include <QWidget>

class SwitchButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal handlePos READ handlePos WRITE setHandlePos)

public:
    explicit SwitchButton(QWidget *parent = nullptr);

    bool isChecked() const { return m_checked; }
    void setChecked(bool checked);

    qreal handlePos() const { return m_pos; }
    void setHandlePos(qreal pos);

signals:
    void toggled(bool checked);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_checked = false;
    qreal m_pos = 0.0;
    QPropertyAnimation *m_animation = nullptr;
};
