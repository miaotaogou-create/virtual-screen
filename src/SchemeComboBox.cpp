#include "SchemeComboBox.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QShowEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionComboBox>

namespace {

constexpr int kLeftPad = 28;
constexpr int kRightPad = 28;
constexpr int kComboHeight = SchemeComboBox::FrameHeight;
constexpr int kCornerRadius = 6;
constexpr int kFontSize = 14;
constexpr int kExtraWidth = 32;
constexpr int kMinWidth = 240;

} // namespace

void SchemeItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QRect itemRect = option.rect.adjusted(3, 2, -3, -2);
    if (option.state & QStyle::State_Selected) {
        painter->setBrush(QColor(6, 182, 212, 40));
        painter->setPen(QPen(QColor(6, 182, 212, 180), 1));
        painter->drawRoundedRect(itemRect, 6, 6);
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setBrush(QColor(30, 41, 59, 150));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(itemRect, 6, 6);
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    painter->setPen(option.state & QStyle::State_Selected ? QColor(QStringLiteral("#38bdf8"))
                                                          : QColor(QStringLiteral("#e2e8f0")));
    painter->setFont(option.font);
    painter->drawText(option.rect.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, text);

    painter->restore();
}

QSize SchemeItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 36);
}

SchemeComboBox::SchemeComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setObjectName(QStringLiteral("SchemeComboBox"));
    setAutoFillBackground(false);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setItemDelegate(new SchemeItemDelegate(this));

    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { refitWidth(); });

    setStyleSheet(QStringLiteral(R"(
        #SchemeComboBox {
            background: transparent;
            border: none;
            padding: 0;
            min-height: 36px;
            max-height: 36px;
        }
        #SchemeComboBox::drop-down {
            border: none;
            width: 0px;
        }
        #SchemeComboBox::down-arrow {
            image: none;
            width: 0px;
            height: 0px;
        }
        #SchemeComboBox QAbstractItemView {
            background-color: #0b1320;
            border: 1px solid #475569;
            border-radius: 8px;
            padding: 4px;
            outline: none;
            selection-background-color: transparent;
        }
    )"));

    lockFrameHeight();
    refitWidth();
}

void SchemeComboBox::lockFrameHeight()
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMinimumHeight(kComboHeight);
    setMaximumHeight(kComboHeight);
    setFixedHeight(kComboHeight);
}

QSize SchemeComboBox::sizeHint() const
{
    return QSize(width() > 0 ? width() : kMinWidth, kComboHeight);
}

QSize SchemeComboBox::minimumSizeHint() const
{
    return QSize(kMinWidth, kComboHeight);
}

void SchemeComboBox::refitWidth()
{
    QFont font;
    font.setPixelSize(kFontSize);
    QFontMetrics plainFm(font);

    QFont boldFont = font;
    boldFont.setBold(true);
    QFontMetrics boldFm(boldFont);

    const int prefixW = plainFm.horizontalAdvance(QStringLiteral("方案:  "));
    const QString name = currentText().isEmpty() ? QStringLiteral("未加载") : currentText();
    const int nameW = boldFm.horizontalAdvance(name);

    setFixedWidth(qMax(kMinWidth, prefixW + nameW + kLeftPad + kRightPad + kExtraWidth));
    lockFrameHeight();
}

void SchemeComboBox::showEvent(QShowEvent *event)
{
    QComboBox::showEvent(event);
    lockFrameHeight();
}

void SchemeComboBox::showPopup()
{
    QComboBox::showPopup();
    if (QWidget *popup = view() ? view()->window() : nullptr) {
        popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        popup->setAttribute(Qt::WA_TranslucentBackground);
    }
}

void SchemeComboBox::changeEvent(QEvent *event)
{
    QComboBox::changeEvent(event);
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)
        update();
}

void SchemeComboBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(rect());

    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    const bool hover = opt.state & QStyle::State_MouseOver;

    QColor bg(0x0b, 0x13, 0x20);
    if (hover)
        bg = QColor(0x0f, 0x1a, 0x2c);

    QColor border(0x47, 0x55, 0x69);
    if (hover)
        border = QColor(0x64, 0x74, 0x8b);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath frame;
    frame.addRoundedRect(body, kCornerRadius, kCornerRadius);

    p.fillPath(frame, bg);
    p.setPen(QPen(border, 1.0));
    p.drawPath(frame);

    const int dotX = 18;
    const int dotY = height() / 2;
    const int dotRadius = 5;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(6, 182, 212, 60));
    p.drawEllipse(QPoint(dotX, dotY), dotRadius + 2, dotRadius + 2);
    p.setBrush(QColor(QStringLiteral("#06b6d4")));
    p.drawEllipse(QPoint(dotX, dotY), dotRadius, dotRadius);

    int textStartX = kLeftPad;
    QFont font = p.font();
    font.setPixelSize(kFontSize);
    font.setBold(false);
    p.setFont(font);
    p.setPen(QColor(QStringLiteral("#94a3b8")));

    const QString prefix = QStringLiteral("方案:  ");
    const QFontMetrics fm(font);
    p.drawText(QRect(textStartX, 0, fm.horizontalAdvance(prefix), height()), Qt::AlignVCenter, prefix);

    textStartX += fm.horizontalAdvance(prefix);
    font.setBold(true);
    p.setFont(font);
    p.setPen(QColor(QStringLiteral("#ffffff")));
    p.drawText(QRect(textStartX, 0, width() - textStartX - kRightPad, height()),
               Qt::AlignVCenter | Qt::AlignLeft, currentText());

    const int arrowRight = width() - 18;
    const int arrowY = height() / 2;
    QPen arrowPen(QColor(QStringLiteral("#94a3b8")), 1.6);
    arrowPen.setCapStyle(Qt::RoundCap);
    arrowPen.setJoinStyle(Qt::RoundJoin);
    p.setPen(arrowPen);

    QPolygonF arrow;
    arrow << QPointF(arrowRight - 5, arrowY - 2.5) << QPointF(arrowRight, arrowY + 2.5)
          << QPointF(arrowRight + 5, arrowY - 2.5);
    p.drawPolyline(arrow);
}
