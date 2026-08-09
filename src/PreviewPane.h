#pragma once

#include <QPixmap>
#include <QWidget>

class QLabel;
class QPushButton;
class QWidget;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

/** 中间预览区：等比显示；支持把鼠标/滚轮/键盘转发到虚拟屏。 */
class PreviewPane : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewPane(QWidget *parent = nullptr);
    void setPixmap(const QPixmap &pm);
    void setPlaceholder(const QString &text);
    void setGuide(const QString &title, const QString &body,
                  const QString &primaryText = QString(),
                  const QString &secondaryText = QString());

signals:
    void primaryClicked();
    void secondaryClicked();
    /** 预览内容区内归一化坐标 (0..1)；button=NoButton 表示移动；wheelDelta 非 0 为滚轮。 */
    void pointerEvent(qreal nx, qreal ny, Qt::MouseButton button, bool pressed, int wheelDelta);
    void keyEvent(int key, Qt::KeyboardModifiers mods, bool pressed);

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;

private:
    void ensureScaled();
    void layoutGuide();
    void showGuidePanel(bool on);
    QRect contentRect() const;
    bool mapToNorm(const QPoint &pos, qreal *nx, qreal *ny) const;
    void emitPointer(const QPoint &pos, Qt::MouseButton button, bool pressed, int wheelDelta = 0);

    QPixmap m_source;
    QPixmap m_scaled;
    QString m_placeholder;
    QWidget *m_guide = nullptr;
    QLabel *m_guideTitle = nullptr;
    QLabel *m_guideBody = nullptr;
    QPushButton *m_primary = nullptr;
    QPushButton *m_secondary = nullptr;
    bool m_dragging = false;
};
