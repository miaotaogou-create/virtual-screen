#include "PropertiesDrawer.h"
#include "SwitchButton.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QPixmap tuneSlidersPixmap(int size = 36)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QSvgRenderer renderer(QStringLiteral(":/icons/icon_tune_sliders.svg"));
    if (!renderer.isValid())
        return pm;
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&p, QRectF(0, 0, size, size));
    return pm;
}

QIcon svgIcon(const QString &path, const QColor &color, int size = 16)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QIcon();
    QString xml = QString::fromUtf8(file.readAll());
    xml.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
    QSvgRenderer renderer(xml.toUtf8());
    if (!renderer.isValid())
        return QIcon();
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, size, size));
    return QIcon(pm);
}

class DrawerXButton : public QPushButton
{
public:
    explicit DrawerXButton(const char *objectName, int size, bool dangerHover = false,
                           QWidget *parent = nullptr)
        : QPushButton(parent)
        , m_dangerHover(dangerHover)
    {
        setObjectName(QString::fromUtf8(objectName));
        setFixedSize(size, size);
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

        if (underMouse() && !m_dangerHover) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 20));
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 6, 6);
        }

        QColor color(0x94, 0xa3, 0xb8);
        if (underMouse()) {
            color = m_dangerHover ? QColor(0xf8, 0x71, 0x71) : QColor(0xff, 0xff, 0xff);
        }

        QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        const qreal inset = width() * 0.32;
        const QRectF box = QRectF(rect());
        p.drawLine(QPointF(box.left() + inset, box.top() + inset),
                   QPointF(box.right() - inset, box.bottom() - inset));
        p.drawLine(QPointF(box.right() - inset, box.top() + inset),
                   QPointF(box.left() + inset, box.bottom() - inset));
    }

private:
    bool m_dangerHover = false;
};

QHBoxLayout *sectionHeaderRow(const QString &title, QLabel **valueLabel, QWidget *parent)
{
    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    auto *lab = new QLabel(title, parent);
    lab->setObjectName(QStringLiteral("DrawerSectionLabel"));
    auto *val = new QLabel(parent);
    val->setObjectName(QStringLiteral("DrawerSectionValue"));
    row->addWidget(lab);
    row->addStretch(1);
    row->addWidget(val);
    *valueLabel = val;
    return row;
}

QFrame *sectionDivider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("DrawerDivider"));
    line->setFixedHeight(1);
    return line;
}

void makeClickThrough(QLabel *lab)
{
    lab->setAttribute(Qt::WA_TransparentForMouseEvents);
}

} // namespace

PropertiesDrawer::PropertiesDrawer(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("PropertiesDrawer"));
    setFixedWidth(320);
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(450);
    connect(m_debounce, &QTimer::timeout, this, &PropertiesDrawer::emitNow);
    setupUi();
    setEnabledDrawer(false);
    clearCapturedApp();
}

void PropertiesDrawer::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("DrawerScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("DrawerBody"));
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(16);

    auto *header = new QHBoxLayout();
    header->setSpacing(8);
    auto *iconLab = new QLabel(body);
    iconLab->setObjectName(QStringLiteral("DrawerIconBox"));
    iconLab->setPixmap(tuneSlidersPixmap(36));
    iconLab->setFixedSize(36, 36);
    iconLab->setAlignment(Qt::AlignCenter);
    iconLab->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    auto *titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("屏幕规格属性"), body);
    title->setObjectName(QStringLiteral("DrawerTitle"));
    auto *sub = new QLabel(QStringLiteral("即时响应生效 · 无需重启"), body);
    sub->setObjectName(QStringLiteral("DrawerSubTitle"));
    titleCol->addWidget(title);
    titleCol->addWidget(sub);
    auto *closeBtn = new DrawerXButton("DrawerCloseBtn", 28, false, body);
    header->addWidget(iconLab, 0, Qt::AlignVCenter);
    header->addLayout(titleCol, 1);
    header->addWidget(closeBtn, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    layout->addWidget(sectionDivider(body));

    auto *aliasLab = new QLabel(QStringLiteral("显示器别名"), body);
    aliasLab->setObjectName(QStringLiteral("DrawerFieldLabel"));
    layout->addWidget(aliasLab);
    m_nameEdit = new QLineEdit(body);
    m_nameEdit->setObjectName(QStringLiteral("DrawerInput"));
    m_nameEdit->setPlaceholderText(QStringLiteral("屏 1"));
    layout->addWidget(m_nameEdit);

    layout->addLayout(sectionHeaderRow(QStringLiteral("分辨率预设"), &m_presetValue, body));
    m_presetCombo = new QComboBox(body);
    m_presetCombo->setObjectName(QStringLiteral("DrawerCombo"));
    m_presetCombo->addItem(QStringLiteral("1080p (FHD 16:9) (1920×1080)"), QSize(1920, 1080));
    m_presetCombo->addItem(QStringLiteral("1200p (WUXGA 16:10) (1920×1200)"), QSize(1920, 1200));
    m_presetCombo->addItem(QStringLiteral("2K (QHD 16:9) (2560×1440)"), QSize(2560, 1440));
    m_presetCombo->addItem(QStringLiteral("4K (UHD 16:9) (3840×2160)"), QSize(3840, 2160));
    m_presetCombo->addItem(QStringLiteral("竖屏 (1080×1920)"), QSize(1080, 1920));
    layout->addWidget(m_presetCombo);

    auto *whRow = new QHBoxLayout();
    whRow->setSpacing(10);
    auto *wCol = new QVBoxLayout();
    wCol->setSpacing(6);
    auto *wLab = new QLabel(QStringLiteral("宽度 (px)"), body);
    wLab->setObjectName(QStringLiteral("DrawerFieldLabel"));
    m_widthSpin = new QSpinBox(body);
    m_widthSpin->setObjectName(QStringLiteral("DrawerSpin"));
    m_widthSpin->setRange(640, 7680);
    wCol->addWidget(wLab);
    wCol->addWidget(m_widthSpin);
    auto *hCol = new QVBoxLayout();
    hCol->setSpacing(6);
    auto *hLab = new QLabel(QStringLiteral("高度 (px)"), body);
    hLab->setObjectName(QStringLiteral("DrawerFieldLabel"));
    m_heightSpin = new QSpinBox(body);
    m_heightSpin->setObjectName(QStringLiteral("DrawerSpin"));
    m_heightSpin->setRange(480, 4320);
    hCol->addWidget(hLab);
    hCol->addWidget(m_heightSpin);
    whRow->addLayout(wCol, 1);
    whRow->addLayout(hCol, 1);
    layout->addLayout(whRow);

    layout->addWidget(sectionDivider(body));

    layout->addLayout(sectionHeaderRow(QStringLiteral("刷新率 (HZ)"), &m_hzValue, body));
    auto *hzGrid = new QGridLayout();
    hzGrid->setSpacing(6);
    m_hzGroup = new QButtonGroup(this);
    m_hzGroup->setExclusive(true);
    const int hzRates[] = {30, 60, 75, 90, 120, 144, 165, 240};
    for (int i = 0; i < 8; ++i) {
        auto *btn = new QPushButton(QStringLiteral("%1Hz").arg(hzRates[i]), body);
        btn->setObjectName(QStringLiteral("DrawerOptionBtn"));
        btn->setCheckable(true);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        m_hzGroup->addButton(btn, hzRates[i]);
        hzGrid->addWidget(btn, i / 4, i % 4);
    }
    layout->addLayout(hzGrid);

    layout->addLayout(sectionHeaderRow(QStringLiteral("DPI 缩放比例"), &m_scaleValue, body));
    auto *scaleGrid = new QGridLayout();
    scaleGrid->setSpacing(6);
    m_scaleGroup = new QButtonGroup(this);
    m_scaleGroup->setExclusive(true);
    const int scales[] = {100, 125, 150, 175, 200, 250};
    for (int i = 0; i < 6; ++i) {
        auto *btn = new QPushButton(QStringLiteral("%1%").arg(scales[i]), body);
        btn->setObjectName(QStringLiteral("DrawerOptionBtn"));
        btn->setCheckable(true);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        m_scaleGroup->addButton(btn, scales[i]);
        scaleGrid->addWidget(btn, i / 3, i % 3);
    }
    layout->addLayout(scaleGrid);

    layout->addWidget(sectionDivider(body));

    auto *orientLab = new QLabel(QStringLiteral("方向与高级特性"), body);
    orientLab->setObjectName(QStringLiteral("DrawerFieldLabel"));
    layout->addWidget(orientLab);
    auto *orientRow = new QHBoxLayout();
    orientRow->setSpacing(8);
    m_landscapeBtn = new QPushButton(body);
    m_landscapeBtn->setObjectName(QStringLiteral("DrawerOrientBtn"));
    m_landscapeBtn->setCheckable(true);
    m_landscapeBtn->setChecked(true);
    m_landscapeBtn->setFixedHeight(48);
    m_landscapeBtn->setCursor(Qt::PointingHandCursor);
    auto *landLay = new QVBoxLayout(m_landscapeBtn);
    landLay->setContentsMargins(10, 8, 10, 8);
    landLay->setSpacing(2);
    auto *landTitle = new QLabel(QStringLiteral("横向显示"), m_landscapeBtn);
    landTitle->setObjectName(QStringLiteral("DrawerOrientTitle"));
    auto *landSub = new QLabel(QStringLiteral("标准宽屏模式"), m_landscapeBtn);
    landSub->setObjectName(QStringLiteral("DrawerOrientSub"));
    makeClickThrough(landTitle);
    makeClickThrough(landSub);
    landLay->addWidget(landTitle);
    landLay->addWidget(landSub);
    m_portraitBtn = new QPushButton(body);
    m_portraitBtn->setObjectName(QStringLiteral("DrawerOrientBtn"));
    m_portraitBtn->setCheckable(true);
    m_portraitBtn->setFixedHeight(48);
    m_portraitBtn->setCursor(Qt::PointingHandCursor);
    auto *portLay = new QVBoxLayout(m_portraitBtn);
    portLay->setContentsMargins(10, 8, 10, 8);
    portLay->setSpacing(2);
    auto *portTitle = new QLabel(QStringLiteral("纵向竖屏"), m_portraitBtn);
    portTitle->setObjectName(QStringLiteral("DrawerOrientTitle"));
    auto *portSub = new QLabel(QStringLiteral("代码/文档阅读"), m_portraitBtn);
    portSub->setObjectName(QStringLiteral("DrawerOrientSub"));
    makeClickThrough(portTitle);
    makeClickThrough(portSub);
    portLay->addWidget(portTitle);
    portLay->addWidget(portSub);
    orientRow->addWidget(m_landscapeBtn, 1);
    orientRow->addWidget(m_portraitBtn, 1);
    layout->addLayout(orientRow);

    auto *togglePanel = new QFrame(body);
    togglePanel->setObjectName(QStringLiteral("DrawerTogglePanel"));
    auto *toggleLay = new QVBoxLayout(togglePanel);
    toggleLay->setContentsMargins(10, 8, 10, 8);
    toggleLay->setSpacing(10);
    auto *hdrRow = new QHBoxLayout();
    auto *hdrLabel = new QLabel(QStringLiteral("HDR 高动态范围"), togglePanel);
    hdrLabel->setObjectName(QStringLiteral("DrawerToggleLabel"));
    m_hdrSwitch = new SwitchButton(togglePanel);
    m_hdrSwitch->setChecked(false);
    hdrRow->addWidget(hdrLabel);
    hdrRow->addStretch();
    hdrRow->addWidget(m_hdrSwitch);
    toggleLay->addLayout(hdrRow);
    auto *primaryRow = new QHBoxLayout();
    auto *primaryLabel = new QLabel(QStringLiteral("设为 Windows 主屏"), togglePanel);
    primaryLabel->setObjectName(QStringLiteral("DrawerToggleLabel"));
    m_primarySwitch = new SwitchButton(togglePanel);
    m_primarySwitch->setChecked(true);
    primaryRow->addWidget(primaryLabel);
    primaryRow->addStretch();
    primaryRow->addWidget(m_primarySwitch);
    toggleLay->addLayout(primaryRow);
    layout->addWidget(togglePanel);

    layout->addWidget(sectionDivider(body));

    auto *castHeader = new QHBoxLayout();
    auto *castLab = new QLabel(QStringLiteral("当前屏幕捕获/投放应用"), body);
    castLab->setObjectName(QStringLiteral("DrawerFieldLabel"));
    m_castNewBtn = new QPushButton(QStringLiteral(" 投放新窗口"), body);
    m_castNewBtn->setObjectName(QStringLiteral("DrawerCastLink"));
    m_castNewBtn->setIcon(svgIcon(QStringLiteral(":/icons/icon_cast_display.svg"),
                                  QColor(0x38, 0xbd, 0xf8), 14));
    m_castNewBtn->setIconSize(QSize(14, 14));
    m_castNewBtn->setCursor(Qt::PointingHandCursor);
    m_castNewBtn->setFlat(true);
    castHeader->addWidget(castLab);
    castHeader->addStretch(1);
    castHeader->addWidget(m_castNewBtn);
    layout->addLayout(castHeader);

    m_castEmpty = new QFrame(body);
    m_castEmpty->setObjectName(QStringLiteral("DrawerCastEmpty"));
    auto *castEmptyLay = new QHBoxLayout(m_castEmpty);
    castEmptyLay->setContentsMargins(12, 10, 12, 10);
    auto *castHint = new QLabel(QStringLiteral("暂无投放窗口"), m_castEmpty);
    castHint->setObjectName(QStringLiteral("DrawerCastHint"));
    castEmptyLay->addWidget(castHint);
    layout->addWidget(m_castEmpty);

    m_castAppCard = new QFrame(body);
    m_castAppCard->setObjectName(QStringLiteral("DrawerCastAppCard"));
    auto *appCardLay = new QHBoxLayout(m_castAppCard);
    appCardLay->setContentsMargins(8, 6, 8, 6);
    appCardLay->setSpacing(8);
    auto *appInfoCol = new QVBoxLayout();
    appInfoCol->setSpacing(1);
    m_castAppTitle = new QLabel(m_castAppCard);
    m_castAppTitle->setObjectName(QStringLiteral("DrawerCastAppTitle"));
    m_castAppExe = new QLabel(m_castAppCard);
    m_castAppExe->setObjectName(QStringLiteral("DrawerCastAppExe"));
    appInfoCol->addWidget(m_castAppTitle);
    appInfoCol->addWidget(m_castAppExe);
    m_castDetachBtn = new DrawerXButton("DrawerDetachBtn", 20, true, m_castAppCard);
    appCardLay->addLayout(appInfoCol, 1);
    appCardLay->addWidget(m_castDetachBtn, 0, Qt::AlignVCenter);
    layout->addWidget(m_castAppCard);

    layout->addStretch(1);
    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    m_deleteBtn = new QPushButton(QStringLiteral(" 移除此虚拟显示器"), this);
    m_deleteBtn->setObjectName(QStringLiteral("DrawerDeleteBtn"));
    m_deleteBtn->setIcon(svgIcon(QStringLiteral(":/icons/icon_trash.svg"), QColor(0xf8, 0x71, 0x71), 16));
    m_deleteBtn->setIconSize(QSize(16, 16));
    m_deleteBtn->setFixedHeight(38);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    auto *foot = new QHBoxLayout();
    foot->setContentsMargins(16, 8, 16, 16);
    foot->addWidget(m_deleteBtn);
    root->addLayout(foot);

    auto *orientGroup = new QButtonGroup(this);
    orientGroup->setExclusive(true);
    orientGroup->addButton(m_landscapeBtn);
    orientGroup->addButton(m_portraitBtn);

    auto hook = [this]() { scheduleEmit(); };

    connect(closeBtn, &QPushButton::clicked, this, &PropertiesDrawer::closeRequested);
    connect(m_nameEdit, &QLineEdit::textEdited, this, hook);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_block)
            return;
        const QSize sz = m_presetCombo->itemData(idx).toSize();
        if (sz.isValid()) {
            m_block = true;
            m_widthSpin->setValue(sz.width());
            m_heightSpin->setValue(sz.height());
            m_landscapeBtn->setChecked(sz.width() >= sz.height());
            m_portraitBtn->setChecked(sz.width() < sz.height());
            m_block = false;
        }
        updateValueLabels();
        scheduleEmit();
    });
    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_block) {
            syncPresetComboFromSize();
            updateValueLabels();
            scheduleEmit();
        }
    });
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_block) {
            syncPresetComboFromSize();
            updateValueLabels();
            scheduleEmit();
        }
    });
    connect(m_hzGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int) {
        updateValueLabels();
        scheduleEmit();
    });
    connect(m_scaleGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int) {
        updateValueLabels();
        scheduleEmit();
    });
    connect(m_landscapeBtn, &QPushButton::clicked, this, [this]() {
        if (m_block || m_widthSpin->value() >= m_heightSpin->value())
            return;
        m_block = true;
        const int w = m_widthSpin->value();
        m_widthSpin->setValue(m_heightSpin->value());
        m_heightSpin->setValue(w);
        m_block = false;
        syncPresetComboFromSize();
        updateValueLabels();
        scheduleEmit();
    });
    connect(m_portraitBtn, &QPushButton::clicked, this, [this]() {
        if (m_block || m_widthSpin->value() <= m_heightSpin->value())
            return;
        m_block = true;
        const int w = m_widthSpin->value();
        m_widthSpin->setValue(m_heightSpin->value());
        m_heightSpin->setValue(w);
        m_block = false;
        syncPresetComboFromSize();
        updateValueLabels();
        scheduleEmit();
    });
    connect(m_castNewBtn, &QPushButton::clicked, this, [this]() {
        if (m_index >= 0)
            emit castRequested(m_index);
    });
    connect(m_castDetachBtn, &QPushButton::clicked, this, [this]() {
        if (m_index >= 0)
            emit castDetachRequested(m_index);
    });
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        if (m_index >= 0)
            emit removeRequested(m_index);
    });
}

void PropertiesDrawer::selectHz(int hz)
{
    if (QAbstractButton *b = m_hzGroup->button(hz))
        b->setChecked(true);
    else if (QAbstractButton *b = m_hzGroup->button(60))
        b->setChecked(true);
}

void PropertiesDrawer::selectScale(int scale)
{
    if (QAbstractButton *b = m_scaleGroup->button(scale))
        b->setChecked(true);
    else if (QAbstractButton *b = m_scaleGroup->button(125))
        b->setChecked(true);
}

void PropertiesDrawer::syncPresetComboFromSize()
{
    const QSize cur(m_widthSpin->value(), m_heightSpin->value());
    int match = -1;
    for (int i = 0; i < m_presetCombo->count(); ++i) {
        if (m_presetCombo->itemData(i).toSize() == cur) {
            match = i;
            break;
        }
    }
    m_block = true;
    m_presetCombo->setCurrentIndex(match >= 0 ? match : -1);
    m_landscapeBtn->setChecked(cur.width() >= cur.height());
    m_portraitBtn->setChecked(cur.width() < cur.height());
    m_block = false;
}

void PropertiesDrawer::updateValueLabels()
{
    m_presetValue->setText(
        QStringLiteral("%1 × %2").arg(m_widthSpin->value()).arg(m_heightSpin->value()));
    const int hz = m_hzGroup->checkedId() > 0 ? m_hzGroup->checkedId() : 60;
    m_hzValue->setText(QStringLiteral("%1 Hz").arg(hz));
    const int scale = m_scaleGroup->checkedId() > 0 ? m_scaleGroup->checkedId() : 125;
    m_scaleValue->setText(QStringLiteral("%1%").arg(scale));
}

void PropertiesDrawer::updateCastVisibility()
{
    const bool hasApp = m_castAppCard && !m_castAppTitle->text().isEmpty();
    m_castAppCard->setVisible(hasApp);
    m_castEmpty->setVisible(!hasApp);
}

void PropertiesDrawer::setCapturedApp(const QString &title, const QString &exe)
{
    m_castAppTitle->setText(title);
    m_castAppExe->setText(exe);
    updateCastVisibility();
}

void PropertiesDrawer::clearCapturedApp()
{
    m_castAppTitle->clear();
    m_castAppExe->clear();
    updateCastVisibility();
}

void PropertiesDrawer::loadDisplay(int index, const DisplaySpec &spec, bool hasVirtual)
{
    Q_UNUSED(hasVirtual);
    m_index = index;
    m_block = true;
    m_nameEdit->setText(spec.label);
    m_widthSpin->setValue(spec.width);
    m_heightSpin->setValue(spec.height);
    selectHz(spec.hz);
    selectScale(spec.scale);
    syncPresetComboFromSize();
    updateValueLabels();
    m_block = false;
    setEnabledDrawer(index >= 0);
}

void PropertiesDrawer::setEnabledDrawer(bool on)
{
    m_nameEdit->setEnabled(on);
    m_presetCombo->setEnabled(on);
    m_widthSpin->setEnabled(on);
    m_heightSpin->setEnabled(on);
    for (QAbstractButton *b : m_hzGroup->buttons())
        b->setEnabled(on);
    for (QAbstractButton *b : m_scaleGroup->buttons())
        b->setEnabled(on);
    m_landscapeBtn->setEnabled(on);
    m_portraitBtn->setEnabled(on);
    m_hdrSwitch->setEnabled(on);
    m_primarySwitch->setEnabled(on);
    m_castNewBtn->setEnabled(on);
    m_castDetachBtn->setEnabled(on);
    m_deleteBtn->setEnabled(on);
}

void PropertiesDrawer::scheduleEmit()
{
    if (m_block || m_index < 0)
        return;
    m_debounce->start();
}

DisplaySpec PropertiesDrawer::readForm() const
{
    DisplaySpec s;
    s.label = m_nameEdit->text().trimmed();
    if (s.label.isEmpty())
        s.label = QStringLiteral("屏%1").arg(m_index + 1);
    s.width = m_widthSpin->value();
    s.height = m_heightSpin->value();
    s.hz = m_hzGroup->checkedId() > 0 ? m_hzGroup->checkedId() : 60;
    s.scale = m_scaleGroup->checkedId() > 0 ? m_scaleGroup->checkedId() : 125;
    return s;
}

void PropertiesDrawer::emitNow()
{
    if (m_index < 0)
        return;
    emit displayEdited(m_index, readForm());
}
