#pragma once

#include <QPixmap>
#include <QWidget>

class QLabel;
class QPushButton;
class QWidget;

/** 中间预览区：等比铺满；空闲时显示步骤引导。 */
class PreviewPane : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewPane(QWidget *parent = nullptr);
    void setPixmap(const QPixmap &pm);
    void setPlaceholder(const QString &text);
    /** 居中步骤卡；按钮文案为空则隐藏对应按钮。 */
    void setGuide(const QString &title, const QString &body,
                  const QString &primaryText = QString(),
                  const QString &secondaryText = QString());

signals:
    void primaryClicked();
    void secondaryClicked();

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void ensureScaled();
    void layoutGuide();
    void showGuidePanel(bool on);

    QPixmap m_source;
    QPixmap m_scaled;
    QString m_placeholder;
    QWidget *m_guide = nullptr;
    QLabel *m_guideTitle = nullptr;
    QLabel *m_guideBody = nullptr;
    QPushButton *m_primary = nullptr;
    QPushButton *m_secondary = nullptr;
};
