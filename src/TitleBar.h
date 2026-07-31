#pragma once

#include <QWidget>

class QLabel;
class ChromeButton;

/** 窄标题栏：高度约 28px；含最小化 / 最大化(还原) / 关闭。 */
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);
    void syncMaxButton();

signals:
    void applyClicked();
    void clearClicked();
    void settingsClicked();
    void closeClicked();

public slots:
    void setAdminHint(const QString &text);
    void setStatusHint(const QString &text);

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void toggleMaxRestore();

    QLabel *m_title = nullptr;
    QLabel *m_hint = nullptr;
    ChromeButton *m_maxBtn = nullptr;
    QPoint m_dragPos;
    bool m_dragging = false;
};
