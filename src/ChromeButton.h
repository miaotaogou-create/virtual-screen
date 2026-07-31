#pragma once

#include <QAbstractButton>

/** 标题栏窗控：自绘图标，线宽/占位统一。 */
class ChromeButton : public QAbstractButton
{
    Q_OBJECT
public:
    enum Kind { Minimize, Maximize, Restore, Close };

    explicit ChromeButton(Kind kind, QWidget *parent = nullptr);
    void setKind(Kind kind);
    Kind kind() const { return m_kind; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    Kind m_kind;
    bool m_hover = false;
};
