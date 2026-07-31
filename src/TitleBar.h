#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

/** 窄标题栏：高度约 28px，少占预览区。 */
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

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

private:
    QLabel *m_title = nullptr;
    QLabel *m_hint = nullptr;
    QPoint m_dragPos;
    bool m_dragging = false;
};
