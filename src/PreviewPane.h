#pragma once

#include <QPixmap>
#include <QWidget>

/** 中间预览区：等比铺满（类似 VMware Fit），黑底。 */
class PreviewPane : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewPane(QWidget *parent = nullptr);
    void setPixmap(const QPixmap &pm);
    void setPlaceholder(const QString &text);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QPixmap m_pm;
    QString m_placeholder;
};
