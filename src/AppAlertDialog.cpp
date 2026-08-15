#include "AppAlertDialog.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

class AlertCloseButton : public QPushButton
{
public:
    explicit AlertCloseButton(QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        setObjectName(QStringLiteral("AppAlertCloseBtn"));
        setFixedSize(24, 24);
        setFlat(true);
        setText(QString());
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        if (underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 20));
            p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
        }

        QColor color(0x94, 0xa3, 0xb8);
        if (underMouse())
            color = QColor(0xff, 0xff, 0xff);

        QPen pen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        const qreal inset = width() * 0.30;
        const QRectF box = QRectF(rect());
        p.drawLine(QPointF(box.left() + inset, box.top() + inset),
                   QPointF(box.right() - inset, box.bottom() - inset));
        p.drawLine(QPointF(box.right() - inset, box.top() + inset),
                   QPointF(box.left() + inset, box.bottom() - inset));
    }

    void enterEvent(QEvent *event) override
    {
        QPushButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override
    {
        QPushButton::leaveEvent(event);
        update();
    }
};

class AlertKindIcon : public QWidget
{
public:
    explicit AlertKindIcon(AppAlertDialog::Kind kind, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_kind(kind)
    {
        setFixedSize(22, 22);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QColor fill(0x06, 0xb6, 0xd4);
        if (m_kind == AppAlertDialog::Warning)
            fill = QColor(0xf5, 0x9e, 0x0b);

        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawEllipse(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));

        p.setPen(QColor(0x02, 0x06, 0x17));
        QFont f = font();
        f.setPixelSize(12);
        f.setBold(true);
        p.setFont(f);
        QString glyph = QStringLiteral("i");
        if (m_kind == AppAlertDialog::Warning)
            glyph = QStringLiteral("!");
        else if (m_kind == AppAlertDialog::Question)
            glyph = QStringLiteral("?");
        p.drawText(rect(), Qt::AlignCenter, glyph);
    }

private:
    AppAlertDialog::Kind m_kind;
};

} // namespace

AppAlertDialog::AppAlertDialog(Kind kind, const QString &title, const QString &text,
                               bool showCancel, const QString &okText,
                               const QString &cancelText, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("AppAlertDialog"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::MSWindowsFixedSizeDialogHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setModal(true);
    setFixedWidth(440);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *root = new QVBoxLayout(this);
    // 提示框宜紧凑：顶/中留白压到接近「标题贴顶」
    root->setContentsMargins(16, 10, 12, 12);
    root->setSpacing(8);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("AppAlertHeader"));
    header->setFixedHeight(24);
    auto *headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(0, 0, 0, 0);
    headerLay->setSpacing(8);
    headerLay->addWidget(new AlertKindIcon(kind, header), 0, Qt::AlignVCenter);

    auto *titleLab = new QLabel(title, header);
    titleLab->setObjectName(QStringLiteral("AppAlertTitle"));
    titleLab->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    headerLay->addWidget(titleLab, 1, Qt::AlignVCenter);

    auto *closeBtn = new AlertCloseButton(header);
    headerLay->addWidget(closeBtn, 0, Qt::AlignVCenter);
    root->addWidget(header, 0, Qt::AlignTop);

    auto *body = new QLabel(text, this);
    body->setObjectName(QStringLiteral("AppAlertBody"));
    body->setWordWrap(true);
    body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    root->addWidget(body, 0, Qt::AlignTop);

    auto *foot = new QHBoxLayout();
    foot->setContentsMargins(0, 4, 0, 0);
    foot->setSpacing(8);
    foot->addStretch(1);

    if (showCancel) {
        auto *cancel = new QPushButton(
            cancelText.isEmpty() ? QStringLiteral("取消") : cancelText, this);
        cancel->setObjectName(QStringLiteral("AppAlertCancelBtn"));
        cancel->setCursor(Qt::PointingHandCursor);
        cancel->setFixedHeight(36);
        cancel->setMinimumWidth(88);
        foot->addWidget(cancel);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    auto *ok = new QPushButton(okText.isEmpty() ? QStringLiteral("确定") : okText, this);
    ok->setObjectName(QStringLiteral("AppAlertOkBtn"));
    ok->setCursor(Qt::PointingHandCursor);
    ok->setFixedHeight(36);
    ok->setMinimumWidth(96);
    ok->setDefault(true);
    foot->addWidget(ok);
    root->addLayout(foot);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);

    // 先按固定宽度算出正文真实高度，再锁窗高，避免 wordWrap 未定宽时把对话框撑出大片空白
    const int contentW = 440 - 16 - 12;
    body->setFixedWidth(contentW);
    const int bodyH = qMax(body->heightForWidth(contentW), body->sizeHint().height());
    body->setFixedHeight(bodyH);

    const int footH = 4 + 36;
    const int totalH = 10 + 24 + 8 + bodyH + footH + 12;
    setFixedHeight(totalH);
}

void AppAlertDialog::information(QWidget *parent, const QString &title, const QString &text)
{
    AppAlertDialog(Information, title, text, false, QString(), QString(), parent).exec();
}

void AppAlertDialog::warning(QWidget *parent, const QString &title, const QString &text)
{
    AppAlertDialog(Warning, title, text, false, QString(), QString(), parent).exec();
}

bool AppAlertDialog::question(QWidget *parent, const QString &title, const QString &text,
                              const QString &okText, const QString &cancelText)
{
    return AppAlertDialog(Question, title, text, true, okText, cancelText, parent).exec()
        == QDialog::Accepted;
}
