#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class ChromeButton;

/** Fluent 无边框标题栏：应用名、状态、窗口按钮。 */
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);
    void syncMaxButton();
    void setBusy(bool busy);
    void setDriverReady(bool ready, int virtualCount);

signals:
    void closeClicked();

public slots:
    void setStatusHint(const QString &text);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void toggleMaxRestore();

    QLabel *m_statusDot = nullptr;
    QLabel *m_statusText = nullptr;
    QWidget *m_statusPill = nullptr;
    ChromeButton *m_maxBtn = nullptr;
    QPoint m_dragPos;
    bool m_dragging = false;
};
