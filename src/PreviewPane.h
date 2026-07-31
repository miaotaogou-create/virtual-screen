#pragma once

#include <QPixmap>
#include <QWidget>

/** 中间预览区：等比铺满；缩放只做一次，避免每次 paint 平滑缩放拖垮 CPU。 */
class PreviewPane : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewPane(QWidget *parent = nullptr);
    void setPixmap(const QPixmap &pm);
    void setPlaceholder(const QString &text);

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    void ensureScaled();

    QPixmap m_source;
    QPixmap m_scaled;
    QString m_placeholder;
};
