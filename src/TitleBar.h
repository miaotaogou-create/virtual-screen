#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class ChromeButton;

/** 窄标题栏：状态 + 清除全部 + 窗口按钮。 */
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);
    void syncMaxButton();
    void setBusy(bool busy);

signals:
    void clearClicked();
    void closeClicked();

public slots:
    void setStatusHint(const QString &text);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void toggleMaxRestore();

    QLabel *m_hint = nullptr;
    QPushButton *m_clear = nullptr;
    ChromeButton *m_maxBtn = nullptr;
    QPoint m_dragPos;
    bool m_dragging = false;
};
