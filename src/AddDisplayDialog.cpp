#include "AddDisplayDialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QLabel *fieldCaption(const QString &text, QWidget *parent)
{
    auto *lab = new QLabel(text, parent);
    lab->setObjectName(QStringLiteral("AddDlgCaption"));
    return lab;
}

/** 青色圆底 + 号，自绘保证几何居中。 */
class AddPlusIcon : public QWidget
{
public:
    explicit AddPlusIcon(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(28, 28);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x06, 0xb6, 0xd4));
        p.drawEllipse(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));

        QPen pen(QColor(0x02, 0x06, 0x17), 2.4, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        const QPointF c = QRectF(rect()).center();
        const qreal arm = 5.5;
        p.drawLine(QPointF(c.x() - arm, c.y()), QPointF(c.x() + arm, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - arm), QPointF(c.x(), c.y() + arm));
    }
};

/** 关闭 ×：自绘，避免字体偏移。 */
class AddCloseButton : public QPushButton
{
public:
    explicit AddCloseButton(QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        setObjectName(QStringLiteral("AddDlgCloseBtn"));
        setFixedSize(28, 28);
        setFlat(true);
        setText(QString());
        setCursor(Qt::PointingHandCursor);
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
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 6, 6);
        }

        QColor color(0x94, 0xa3, 0xb8);
        if (underMouse())
            color = QColor(0xff, 0xff, 0xff);

        QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        const qreal inset = width() * 0.32;
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

} // namespace

AddDisplayDialog::AddDisplayDialog(int ordinal, QWidget *parent)
    : QDialog(parent)
    , m_ordinal(ordinal)
{
    setObjectName(QStringLiteral("AddDisplayDialog"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(520);
    setModal(true);

    m_spec = DisplaySpec{};
    m_spec.label = QStringLiteral("屏%1").arg(ordinal);
    m_spec.width = 1920;
    m_spec.height = 1080;
    m_spec.hz = 144;
    m_spec.scale = 125;

    setupUi();
    applyTemplate(0);
}

void AddDisplayDialog::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(18);

    // 顶栏：青色 + 圆标 + 标题 · 右侧关闭 ×
    auto *header = new QHBoxLayout();
    header->setSpacing(10);

    auto *icon = new AddPlusIcon(this);

    auto *title = new QLabel(QStringLiteral("添加新的虚拟显示器"), this);
    title->setObjectName(QStringLiteral("AddDlgTitle"));

    auto *closeBtn = new AddCloseButton(this);

    header->addWidget(icon, 0, Qt::AlignVCenter);
    header->addWidget(title, 0, Qt::AlignVCenter);
    header->addStretch(1);
    header->addWidget(closeBtn, 0, Qt::AlignVCenter);
    root->addLayout(header);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    // 快捷模板
    root->addWidget(fieldCaption(QStringLiteral("常用规格快捷模板"), this));

    auto *tplRow = new QHBoxLayout();
    tplRow->setSpacing(10);
    m_tpl1080 = new QPushButton(QStringLiteral("1080P FHD (16:9)"), this);
    m_tpl2k = new QPushButton(QStringLiteral("2K QHD (2560x1440)"), this);
    m_tpl4k = new QPushButton(QStringLiteral("4K UHD (3840x2160)"), this);
    m_tplGroup = new QButtonGroup(this);
    m_tplGroup->setExclusive(true);
    int i = 0;
    for (QPushButton *b : {m_tpl1080, m_tpl2k, m_tpl4k}) {
        b->setObjectName(QStringLiteral("AddDlgTplBtn"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(40);
        m_tplGroup->addButton(b, i++);
        tplRow->addWidget(b, 1);
    }
    root->addLayout(tplRow);

    // 分辨率 · 刷新率 双列
    auto *paramRow = new QHBoxLayout();
    paramRow->setSpacing(14);

    auto *resCol = new QVBoxLayout();
    resCol->setSpacing(8);
    resCol->addWidget(fieldCaption(QStringLiteral("分辨率 (宽 × 高)"), this));
    auto *resCard = new QFrame(this);
    resCard->setObjectName(QStringLiteral("AddDlgValueCard"));
    auto *resLay = new QHBoxLayout(resCard);
    resLay->setContentsMargins(14, 0, 14, 0);
    m_resValue = new QLabel(resCard);
    m_resValue->setObjectName(QStringLiteral("AddDlgValueText"));
    resLay->addWidget(m_resValue);
    resCard->setFixedHeight(40);
    resCol->addWidget(resCard);

    auto *hzCol = new QVBoxLayout();
    hzCol->setSpacing(8);
    hzCol->addWidget(fieldCaption(QStringLiteral("刷新率"), this));
    m_hz = new QComboBox(this);
    m_hz->setObjectName(QStringLiteral("AddDlgHzCombo"));
    m_hz->addItem(QStringLiteral("60 Hz (标准)"), 60);
    m_hz->addItem(QStringLiteral("75 Hz"), 75);
    m_hz->addItem(QStringLiteral("120 Hz"), 120);
    m_hz->addItem(QStringLiteral("144 Hz (电竞高刷)"), 144);
    m_hz->addItem(QStringLiteral("165 Hz"), 165);
    m_hz->addItem(QStringLiteral("240 Hz"), 240);
    m_hz->setFixedHeight(40);
    hzCol->addWidget(m_hz);

    paramRow->addLayout(resCol, 1);
    paramRow->addLayout(hzCol, 1);
    root->addLayout(paramRow);

    // HDR 行（UI 占位，点击切换启用态）
    m_hdrRow = new QFrame(this);
    m_hdrRow->setObjectName(QStringLiteral("AddDlgHdrRow"));
    m_hdrRow->setCursor(Qt::PointingHandCursor);
    m_hdrRow->setFixedHeight(44);
    auto *hdrLay = new QHBoxLayout(m_hdrRow);
    hdrLay->setContentsMargins(14, 0, 14, 0);
    auto *hdrTitle = new QLabel(QStringLiteral("开启 10-bit HDR 高动态色彩"), m_hdrRow);
    hdrTitle->setObjectName(QStringLiteral("AddDlgHdrTitle"));
    m_hdrStatus = new QLabel(m_hdrRow);
    m_hdrStatus->setObjectName(QStringLiteral("AddDlgHdrStatus"));
    hdrLay->addWidget(hdrTitle);
    hdrLay->addStretch(1);
    hdrLay->addWidget(m_hdrStatus);
    root->addWidget(m_hdrRow);
    updateHdrRow(m_hdr);

    root->addStretch(1);

    // 底栏
    auto *foot = new QHBoxLayout();
    foot->setSpacing(10);
    foot->addStretch(1);

    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    cancel->setObjectName(QStringLiteral("AddDlgCancelBtn"));
    cancel->setFixedHeight(44);
    cancel->setMinimumWidth(88);
    cancel->setCursor(Qt::PointingHandCursor);

    auto *ok = new QPushButton(QStringLiteral("立即创建虚拟显示器"), this);
    ok->setObjectName(QStringLiteral("AddDlgCreateBtn"));
    ok->setFixedHeight(44);
    ok->setMinimumWidth(180);
    ok->setCursor(Qt::PointingHandCursor);

    auto *glow = new QGraphicsDropShadowEffect(ok);
    glow->setBlurRadius(22);
    glow->setOffset(0, 4);
    glow->setColor(QColor(6, 182, 212, 160));
    ok->setGraphicsEffect(glow);

    foot->addWidget(cancel);
    foot->addWidget(ok);
    root->addLayout(foot);

    connect(m_tplGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this,
            &AddDisplayDialog::applyTemplate);
    connect(m_hz, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        syncFromForm();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, [this]() {
        syncFromForm();
        accept();
    });

    // 点击 HDR 行切换
    m_hdrRow->installEventFilter(this);
}

bool AddDisplayDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_hdrRow && event->type() == QEvent::MouseButtonRelease) {
        updateHdrRow(!m_hdr);
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void AddDisplayDialog::updateHdrRow(bool on)
{
    m_hdr = on;
    m_hdrStatus->setText(on ? QStringLiteral("已启用") : QStringLiteral("未启用"));
    m_hdrStatus->setProperty("enabled", on);
    m_hdrStatus->style()->unpolish(m_hdrStatus);
    m_hdrStatus->style()->polish(m_hdrStatus);
}

void AddDisplayDialog::updateResolutionLabel()
{
    m_resValue->setText(QStringLiteral("%1 × %2").arg(m_width).arg(m_height));
}

void AddDisplayDialog::applyTemplate(int index)
{
    if (index == 0) {
        m_width = 1920;
        m_height = 1080;
        m_tpl1080->setChecked(true);
    } else if (index == 1) {
        m_width = 2560;
        m_height = 1440;
        m_tpl2k->setChecked(true);
    } else {
        m_width = 3840;
        m_height = 2160;
        m_tpl4k->setChecked(true);
    }
    updateResolutionLabel();
    const int hzIdx = m_hz->findData(144);
    if (hzIdx >= 0)
        m_hz->setCurrentIndex(hzIdx);
    syncFromForm();
}

void AddDisplayDialog::syncFromForm()
{
    m_spec.label = QStringLiteral("屏%1").arg(m_ordinal);
    m_spec.width = m_width;
    m_spec.height = m_height;
    m_spec.hz = m_hz->currentData().toInt();
    if (m_spec.hz <= 0)
        m_spec.hz = 60;
    m_spec.scale = 125;
}
