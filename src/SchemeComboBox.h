#pragma once

#include <QComboBox>
#include <QStyledItemDelegate>

class QPainter;
class QStyleOptionViewItem;
class QModelIndex;

/** 方案下拉列表项：圆角悬停/选中高亮。 */
class SchemeItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

/** 工具栏方案胶囊：青点 + 灰前缀 + 加粗方案名 + Chevron。 */
class SchemeComboBox : public QComboBox
{
    Q_OBJECT
public:
    static constexpr int FrameHeight = 36;

    explicit SchemeComboBox(QWidget *parent = nullptr);
    void refitWidth();
    void lockFrameHeight();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showPopup() override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
};
