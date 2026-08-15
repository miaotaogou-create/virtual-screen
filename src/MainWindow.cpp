#include "MainWindow.h"

#include "AddDisplayDialog.h"
#include "AppAlertDialog.h"
#include "CastWindowDialog.h"
#include "Elevate.h"
#include "PresetHubDialog.h"
#include "PropertiesDrawer.h"
#include "SchemeComboBox.h"
#include "SettingsPanel.h"
#include "TitleBar.h"
#include "VddService.h"

#include <QButtonGroup>
#include <QAbstractButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QShowEvent>
#include <QSvgRenderer>
#include <QSignalBlocker>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

static const char *kDriverReleasesUrl =
    "https://github.com/nomi-san/parsec-vdd/releases";

namespace {

QIcon svgIcon(const QString &resourcePath, const QColor &stroke, int size = 16)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return QIcon();

    QString xml = QString::fromUtf8(file.readAll());
    xml.replace(QStringLiteral("currentColor"), stroke.name(QColor::HexRgb));

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

const QColor kIconMuted(0x94, 0xa3, 0xb8);
const QColor kViewIconActive(0x06, 0xb6, 0xd4);
const QColor kIconRefresh(0xcb, 0xd5, 0xe1);
const QColor kIconTrash(0xf8, 0x71, 0x71);
const QColor kIconWhite(0xff, 0xff, 0xff);

void styleIconBtn(QPushButton *b, const QIcon &icon, int size = 16)
{
    b->setIcon(icon);
    b->setIconSize(QSize(size, size));
    b->setText(QString());
}

void styleTextIconBtn(QPushButton *b, const QIcon &icon, const QString &text, int size = 16)
{
    b->setIcon(icon);
    b->setIconSize(QSize(size, size));
    b->setText(text);
}

QWidget *makeHeaderDivider(QWidget *parent)
{
    auto *line = new QWidget(parent);
    line->setObjectName(QStringLiteral("HeaderDivider"));
    line->setFixedHeight(1);
    return line;
}

/** 方案预设：外框与 SchemeComboBox 同规格，内嵌文件夹线稿。 */
class SchemeHubButton : public QPushButton
{
public:
    explicit SchemeHubButton(QWidget *parent = nullptr)
        : QPushButton(parent)
        , m_side(SchemeComboBox::FrameHeight)
    {
        setObjectName(QStringLiteral("SchemeHubBtn"));
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedSize(m_side, m_side);
        setCursor(Qt::PointingHandCursor);
        setFlat(true);
        setText(QString());
        m_icon.load(QStringLiteral(":/folder_download_icon.svg"));
    }

    QSize sizeHint() const override { return QSize(m_side, m_side); }
    QSize minimumSizeHint() const override { return QSize(m_side, m_side); }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QColor bg(0x0b, 0x13, 0x20);
        if (underMouse())
            bg = QColor(0x0f, 0x1a, 0x2c);

        QColor border(0x47, 0x55, 0x69);
        if (underMouse())
            border = QColor(0x64, 0x74, 0x8b);

        const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath frame;
        frame.addRoundedRect(body, 6.0, 6.0);
        p.fillPath(frame, bg);
        p.setPen(QPen(border, 1.0));
        p.drawPath(frame);

        const int iconSz = qMax(16, m_side - 14);
        const QRect iconRect((width() - iconSz) / 2, (height() - iconSz) / 2, iconSz, iconSz);
        m_icon.render(&p, iconRect);
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

private:
    int m_side;
    QSvgRenderer m_icon;
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setObjectName(QStringLiteral("MainWindow"));
    setMinimumSize(1060, 680);
    resize(1180, 760);

    m_cfg = AppConfig::load();
    m_cfg.displays.clear();
    m_cfg.profileName.clear();
    m_vdd = new VddService(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_title = new TitleBar(this);
    m_title->setAttribute(Qt::WA_StyledBackground, true);
    root->addWidget(m_title);
    root->addWidget(makeHeaderDivider(this));

    setupTopToolbar(root);
    root->addWidget(makeHeaderDivider(this));

    setupMonitorChrome(root);

    auto *work = new QHBoxLayout();
    work->setContentsMargins(0, 0, 0, 0);
    work->setSpacing(0);

    m_canvas = new TopologyCanvas(this);
    m_canvas->setObjectName(QStringLiteral("TopologyCanvas"));
    m_drawer = new PropertiesDrawer(this);
    work->addWidget(m_canvas, 1);
    work->addWidget(m_drawer, 0);
    root->addLayout(work, 1);
    root->addWidget(m_bottomTabBar);

    m_settings = new SettingsDialog(this);
    m_settings->loadFrom(m_cfg);

    connect(m_title, &TitleBar::closeClicked, this, &QWidget::close);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshDisplays);
    connect(m_schemeHubBtn, &QPushButton::clicked, this, &MainWindow::showPresetHub);
    connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::onClear);
    connect(m_placeBtn, &QPushButton::clicked, this, [this]() { onPlaceWindow(m_tabIndex); });
    connect(m_drawerToggle, &QPushButton::toggled, this, [this](bool checked) {
        m_drawer->setVisible(checked);
        m_drawerToggle->setToolTip(checked ? QStringLiteral("收起属性面板")
                                             : QStringLiteral("展开属性面板"));
        m_drawerToggle->setIcon(svgIcon(QStringLiteral(":/icons/icon_sidebar_toggle.svg"),
                                       checked ? kViewIconActive : kIconMuted, 18));
    });
    connect(m_schemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSchemeComboChanged);

    connect(m_canvas, &TopologyCanvas::displayClicked, this, &MainWindow::selectDisplay);
    connect(m_canvas, &TopologyCanvas::expandRequested, this, &MainWindow::showFocusFullscreen);
    connect(m_canvas, &TopologyCanvas::screenshotRequested, this, &MainWindow::saveFocusScreenshot);
    connect(m_canvas, &TopologyCanvas::topologyArranged, this, &MainWindow::onTopologyArranged);
    connect(m_drawer, &PropertiesDrawer::displayEdited, this,
            [this](int index, const DisplaySpec &spec) { updateDisplayAt(index, spec); });
    connect(m_drawer, &PropertiesDrawer::removeRequested, this, &MainWindow::removeDisplayAt);
    connect(m_drawer, &PropertiesDrawer::castRequested, this, &MainWindow::onPlaceWindow);
    connect(m_drawer, &PropertiesDrawer::castDetachRequested, this, [this](int index) {
        if (index >= 0 && index < m_castByDisplay.size())
            m_castByDisplay[index] = {};
        syncDrawerCast();
    });
    connect(m_drawer, &PropertiesDrawer::closeRequested, this, [this]() {
        m_drawerToggle->setChecked(false);
    });

    connect(m_vdd, &VddService::progress, this, [this](const QString &m) {
        m_title->setStatusHint(m);
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshPreview);
    // 预览抓屏间隔：过慢时毫秒时钟等动态内容会像“卡住”
    m_timer->start(qBound(100, m_cfg.previewIntervalMs, 5000));

    rebuildSchemeCombo();
    syncDisplayUi();
    updateDriverUi();
    if (m_vdd->driverReady())
        m_canvas->setPlaceholderText(QStringLiteral("点「+ 添加显示」或从方案预设加载"));
    else
        m_canvas->setPlaceholderText(QStringLiteral("未检测到驱动 · 点「+ 添加显示」可安装"));
}

void MainWindow::setupTopToolbar(QVBoxLayout *root)
{
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName(QStringLiteral("TopToolbar"));
    m_toolbar->setAttribute(Qt::WA_StyledBackground, true);
    m_toolbar->setFixedHeight(56);

    auto *lay = new QHBoxLayout(m_toolbar);
    lay->setContentsMargins(14, 8, 14, 8);
    lay->setSpacing(10);

    // 左：方案胶囊 + 预设中心
    m_schemeCombo = new SchemeComboBox(m_toolbar);
    lay->addWidget(m_schemeCombo, 0, Qt::AlignVCenter);

    m_schemeHubBtn = new SchemeHubButton(m_toolbar);
    m_schemeHubBtn->setToolTip(QStringLiteral("方案预设中心"));
    lay->addWidget(m_schemeHubBtn, 0, Qt::AlignVCenter);

    lay->addStretch(1);

    // 中：视图切换（分段按钮组）
    auto *viewBox = new QWidget(m_toolbar);
    viewBox->setObjectName(QStringLiteral("ViewModeGroup"));
    viewBox->setFixedHeight(36);
    auto *viewLay = new QHBoxLayout(viewBox);
    viewLay->setContentsMargins(1, 1, 1, 1);
    viewLay->setSpacing(1);

    auto *focusBtn = new QPushButton(QStringLiteral("单屏监视"), viewBox);
    auto *topoBtn = new QPushButton(QStringLiteral("多屏拓扑排列"), viewBox);
    auto *gridBtn = new QPushButton(QStringLiteral("全景网格"), viewBox);
    for (QPushButton *b : {focusBtn, topoBtn, gridBtn}) {
        b->setObjectName(QStringLiteral("ViewModeBtn"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(32);
        b->setIconSize(QSize(16, 16));
        viewLay->addWidget(b);
    }
    focusBtn->setChecked(true);
    lay->addWidget(viewBox, 0, Qt::AlignCenter);

    lay->addStretch(1);

    // 右：投放 / 刷新 / 添加 / 清除 / 抽屉
    auto *right = new QHBoxLayout();
    right->setSpacing(8);

    m_placeBtn = new QPushButton(m_toolbar);
    m_placeBtn->setObjectName(QStringLiteral("IconButton"));
    m_placeBtn->setFixedSize(32, 32);
    m_placeBtn->setToolTip(QStringLiteral("投放应用窗口至虚拟屏…"));
    m_placeBtn->setCursor(Qt::PointingHandCursor);
    styleIconBtn(m_placeBtn,
                 svgIcon(QStringLiteral(":/icons/icon_cast_display.svg"), kViewIconActive, 18), 18);
    right->addWidget(m_placeBtn);

    m_refreshBtn = new QPushButton(m_toolbar);
    m_refreshBtn->setObjectName(QStringLiteral("IconButton"));
    m_refreshBtn->setToolTip(QStringLiteral("重新检测与刷新 IddCx 驱动状态"));
    m_refreshBtn->setFixedSize(32, 32);
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    styleIconBtn(m_refreshBtn,
                 svgIcon(QStringLiteral(":/icons/icon_refresh.svg"), kIconRefresh, 16), 16);
    right->addWidget(m_refreshBtn);

    auto *addBtn = new QPushButton(QStringLiteral("添加显示"), m_toolbar);
    addBtn->setObjectName(QStringLiteral("PrimaryAddBtn"));
    addBtn->setFixedHeight(32);
    addBtn->setCursor(Qt::PointingHandCursor);
    styleTextIconBtn(addBtn,
                     svgIcon(QStringLiteral(":/icons/icon_add.svg"), kIconWhite, 14),
                     QStringLiteral("添加显示"), 14);
    right->addWidget(addBtn);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddDisplay);

    m_clearBtn = new QPushButton(QStringLiteral("清除全部"), m_toolbar);
    m_clearBtn->setObjectName(QStringLiteral("DangerClearBtn"));
    m_clearBtn->setToolTip(QStringLiteral("移除所有虚拟屏"));
    m_clearBtn->setFixedHeight(32);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    styleTextIconBtn(m_clearBtn,
                     svgIcon(QStringLiteral(":/icons/icon_trash.svg"), kIconTrash, 14),
                     QStringLiteral("清除全部"), 14);
    right->addWidget(m_clearBtn);

    m_drawerToggle = new QPushButton(m_toolbar);
    m_drawerToggle->setObjectName(QStringLiteral("SidebarToggleBtn"));
    m_drawerToggle->setToolTip(QStringLiteral("收起属性面板"));
    m_drawerToggle->setFixedSize(32, 32);
    m_drawerToggle->setCheckable(true);
    m_drawerToggle->setChecked(true);
    m_drawerToggle->setCursor(Qt::PointingHandCursor);
    styleIconBtn(m_drawerToggle,
                 svgIcon(QStringLiteral(":/icons/icon_sidebar_toggle.svg"), kViewIconActive, 18),
                 18);
    right->addWidget(m_drawerToggle);

    lay->addLayout(right, 0);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(focusBtn);
    group->addButton(topoBtn);
    group->addButton(gridBtn);

    const auto updateViewIcons = [=]() {
        const auto pick = [](QPushButton *btn, const QString &path) {
            return svgIcon(path, btn->isChecked() ? kViewIconActive : kIconMuted);
        };
        focusBtn->setIcon(pick(focusBtn, QStringLiteral(":/icons/icon_focus_tv.svg")));
        topoBtn->setIcon(pick(topoBtn, QStringLiteral(":/icons/icon_topology_move.svg")));
        gridBtn->setIcon(pick(gridBtn, QStringLiteral(":/icons/icon_grid_view.svg")));
    };
    connect(group, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
            [=](QAbstractButton *) { updateViewIcons(); });
    updateViewIcons();

    connect(focusBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->setViewMode(TopologyCanvas::Focus);
        updateMonitorChromeVisibility();
        refreshPreview();
    });
    connect(topoBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->setViewMode(TopologyCanvas::Topology);
        updateMonitorChromeVisibility();
        syncDisplayUi();
    });
    connect(gridBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->setViewMode(TopologyCanvas::Grid);
        updateMonitorChromeVisibility();
        refreshPreview();
    });

    root->addWidget(m_toolbar);
}

void MainWindow::setupMonitorChrome(QVBoxLayout *root)
{
    m_screenInfoBar = new QWidget(this);
    m_screenInfoBar->setObjectName(QStringLiteral("ScreenInfoBar"));
    m_screenInfoBar->setAttribute(Qt::WA_StyledBackground, true);
    m_screenInfoBar->setFixedHeight(36);

    auto *infoLay = new QHBoxLayout(m_screenInfoBar);
    infoLay->setContentsMargins(14, 0, 14, 0);
    infoLay->setSpacing(8);

    auto *dot = new QLabel(m_screenInfoBar);
    dot->setObjectName(QStringLiteral("ScreenStatusDot"));
    dot->setFixedSize(8, 8);

    m_screenInfoLabel = new QLabel(m_screenInfoBar);
    m_screenInfoLabel->setObjectName(QStringLiteral("ScreenInfoText"));

    infoLay->addWidget(dot, 0, Qt::AlignVCenter);
    infoLay->addWidget(m_screenInfoLabel, 0, Qt::AlignVCenter);
    infoLay->addStretch();

    auto *infoRefresh = new QPushButton(m_screenInfoBar);
    infoRefresh->setObjectName(QStringLiteral("MonitorChromeBtn"));
    infoRefresh->setFixedSize(28, 28);
    infoRefresh->setCursor(Qt::PointingHandCursor);
    infoRefresh->setToolTip(QStringLiteral("刷新预览"));
    styleIconBtn(infoRefresh,
                 svgIcon(QStringLiteral(":/icons/icon_refresh.svg"), kIconRefresh, 16), 16);

    auto *infoExpand = new QPushButton(m_screenInfoBar);
    infoExpand->setObjectName(QStringLiteral("MonitorChromeBtn"));
    infoExpand->setFixedSize(28, 28);
    infoExpand->setCursor(Qt::PointingHandCursor);
    infoExpand->setToolTip(QStringLiteral("展开/收起属性面板"));
    styleIconBtn(infoExpand,
                 svgIcon(QStringLiteral(":/icons/icon_expand.svg"), kIconMuted, 16), 16);

    infoLay->addWidget(infoRefresh, 0, Qt::AlignVCenter);
    infoLay->addWidget(infoExpand, 0, Qt::AlignVCenter);

    connect(infoRefresh, &QPushButton::clicked, this, &MainWindow::refreshPreview);
    connect(infoExpand, &QPushButton::clicked, this, [this]() {
        m_drawerToggle->setChecked(!m_drawerToggle->isChecked());
    });

    root->addWidget(m_screenInfoBar);

    m_bottomTabBar = new QWidget(this);
    m_bottomTabBar->setObjectName(QStringLiteral("BottomTabBar"));
    m_bottomTabBar->setAttribute(Qt::WA_StyledBackground, true);
    m_bottomTabBar->setFixedHeight(42);

    auto *bottomLay = new QHBoxLayout(m_bottomTabBar);
    bottomLay->setContentsMargins(10, 0, 10, 0);
    bottomLay->setSpacing(8);

    auto *tabHost = new QWidget(m_bottomTabBar);
    m_bottomTabLayout = new QHBoxLayout(tabHost);
    m_bottomTabLayout->setContentsMargins(0, 0, 0, 0);
    m_bottomTabLayout->setSpacing(6);
    bottomLay->addWidget(tabHost, 0, Qt::AlignVCenter);

    bottomLay->addStretch();

    m_bottomPlaceBtn = new QPushButton(QStringLiteral("投放窗口…"), m_bottomTabBar);
    m_bottomPlaceBtn->setObjectName(QStringLiteral("BottomPlaceBtn"));
    m_bottomPlaceBtn->setFixedHeight(28);
    m_bottomPlaceBtn->setCursor(Qt::PointingHandCursor);
    styleTextIconBtn(m_bottomPlaceBtn,
                     svgIcon(QStringLiteral(":/icons/icon_cast_display.svg"), kViewIconActive, 14),
                     QStringLiteral("投放窗口…"), 14);

    m_bottomRemoveBtn = new QPushButton(m_bottomTabBar);
    m_bottomRemoveBtn->setObjectName(QStringLiteral("BottomRemoveBtn"));
    m_bottomRemoveBtn->setFixedSize(28, 28);
    m_bottomRemoveBtn->setCursor(Qt::PointingHandCursor);
    m_bottomRemoveBtn->setToolTip(QStringLiteral("移除当前虚拟屏"));
    styleIconBtn(m_bottomRemoveBtn,
                 svgIcon(QStringLiteral(":/icons/icon_trash.svg"), kIconTrash, 14), 14);

    bottomLay->addWidget(m_bottomPlaceBtn, 0, Qt::AlignVCenter);
    bottomLay->addWidget(m_bottomRemoveBtn, 0, Qt::AlignVCenter);

    connect(m_bottomPlaceBtn, &QPushButton::clicked, this, [this]() { onPlaceWindow(m_tabIndex); });
    connect(m_bottomRemoveBtn, &QPushButton::clicked, this, [this]() { removeDisplayAt(m_tabIndex); });
}

void MainWindow::rebuildBottomTabs()
{
    if (!m_bottomTabLayout)
        return;

    while (QLayoutItem *item = m_bottomTabLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    for (int i = 0; i < m_cfg.displays.size(); ++i) {
        const DisplaySpec &d = m_cfg.displays[i];
        auto *tab = new QPushButton(
            QStringLiteral("%1 %2×%3").arg(d.label).arg(d.width).arg(d.height), m_bottomTabBar);
        tab->setObjectName(QStringLiteral("DisplayTabBtn"));
        tab->setProperty("active", i == m_tabIndex);
        tab->setCursor(Qt::PointingHandCursor);
        tab->style()->unpolish(tab);
        tab->style()->polish(tab);
        const int idx = i;
        connect(tab, &QPushButton::clicked, this, [this, idx]() { selectDisplay(idx); });
        m_bottomTabLayout->addWidget(tab);
    }

    auto *addTab = new QPushButton(QStringLiteral("+ 添加屏"), m_bottomTabBar);
    addTab->setObjectName(QStringLiteral("DisplayTabAddBtn"));
    addTab->setCursor(Qt::PointingHandCursor);
    connect(addTab, &QPushButton::clicked, this, &MainWindow::onAddDisplay);
    m_bottomTabLayout->addWidget(addTab);
}

void MainWindow::updateScreenInfoBar()
{
    if (!m_screenInfoLabel)
        return;

    if (m_cfg.displays.isEmpty()) {
        m_screenInfoLabel->setText(QStringLiteral("暂无虚拟屏"));
        return;
    }

    m_tabIndex = qBound(0, m_tabIndex, m_cfg.displays.size() - 1);
    const DisplaySpec &d = m_cfg.displays[m_tabIndex];
    m_screenInfoLabel->setText(
        QStringLiteral("%1  |  %2 × %3  ·  缩放 %4%  ·  %5 Hz")
            .arg(d.label)
            .arg(d.width)
            .arg(d.height)
            .arg(d.scale)
            .arg(d.hz));
}

void MainWindow::updateMonitorChromeVisibility()
{
    const bool focus = m_canvas && m_canvas->viewMode() == TopologyCanvas::Focus;
    const bool hasDisplays = !m_cfg.displays.isEmpty();
    if (m_screenInfoBar)
        m_screenInfoBar->setVisible(focus && hasDisplays);
    if (m_bottomTabBar)
        m_bottomTabBar->setVisible(hasDisplays);
}


void MainWindow::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange && m_title)
        m_title->syncMaxButton();
}

int MainWindow::hitTestBorder(const QPoint &pos) const
{
    const int b = 6;
    const int w = width();
    const int h = height();
    const bool left = pos.x() <= b;
    const bool right = pos.x() >= w - b;
    const bool top = pos.y() <= b;
    const bool bottom = pos.y() >= h - b;
    if (top && left)
        return HTTOPLEFT;
    if (top && right)
        return HTTOPRIGHT;
    if (bottom && left)
        return HTBOTTOMLEFT;
    if (bottom && right)
        return HTBOTTOMRIGHT;
    if (left)
        return HTLEFT;
    if (right)
        return HTRIGHT;
    if (top)
        return HTTOP;
    if (bottom)
        return HTBOTTOM;
    return HTCLIENT;
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#if defined(Q_OS_WIN)
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        const MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST && !isMaximized()) {
            const QPoint local = mapFromGlobal(QPoint(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)));
            const int hit = hitTestBorder(local);
            if (hit != HTCLIENT) {
                *result = hit;
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void MainWindow::persistCfg()
{
    m_cfg.save();
    m_settings->loadFrom(m_cfg);
}

DisplaySpec MainWindow::defaultSpec(int ordinal) const
{
    DisplaySpec s;
    s.label = QStringLiteral("屏%1").arg(ordinal);
    s.width = 1920;
    s.height = 1200;
    s.hz = 60;
    s.scale = 125;
    return s;
}

void MainWindow::rebuildSchemeCombo()
{
    QSignalBlocker block(m_schemeCombo);
    m_schemeCombo->clear();
    m_schemeCombo->addItem(QStringLiteral("未加载"), QString());
    for (const QString &path : listProfilePaths()) {
        const QString name = QFileInfo(path).completeBaseName();
        m_schemeCombo->addItem(name, path);
    }
    if (!m_cfg.profileName.isEmpty()) {
        const int idx = m_schemeCombo->findText(m_cfg.profileName);
        if (idx >= 0)
            m_schemeCombo->setCurrentIndex(idx);
    }
    m_schemeCombo->refitWidth();
}

void MainWindow::syncDisplayUi()
{
    m_tabIndex = qBound(0, m_tabIndex, qMax(0, m_cfg.displays.size() - 1));
    m_canvas->setDisplays(m_cfg.displays);
    m_canvas->setVirtuals(matchedVirtuals());
    m_canvas->setSelectedIndex(m_tabIndex);

    if (m_cfg.displays.isEmpty()) {
        m_drawer->setEnabledDrawer(false);
        updateScreenInfoBar();
        rebuildBottomTabs();
        updateMonitorChromeVisibility();
        return;
    }
    const auto virtuals = matchedVirtuals();
    const bool hasVirtual = m_tabIndex < virtuals.size() && !virtuals[m_tabIndex].deviceName.isEmpty();
    m_drawer->loadDisplay(m_tabIndex, m_cfg.displays[m_tabIndex], hasVirtual);
    syncDrawerCast();

    updateScreenInfoBar();
    rebuildBottomTabs();
    updateMonitorChromeVisibility();
    updateDriverUi();
}

void MainWindow::syncDrawerCast()
{
    while (m_castByDisplay.size() > m_cfg.displays.size())
        m_castByDisplay.removeLast();

    if (m_cfg.displays.isEmpty() || m_tabIndex < 0 || m_tabIndex >= m_castByDisplay.size()) {
        m_drawer->clearCapturedApp();
        return;
    }

    const DisplayCastInfo &cast = m_castByDisplay[m_tabIndex];
    if (cast.title.isEmpty())
        m_drawer->clearCapturedApp();
    else
        m_drawer->setCapturedApp(cast.title, cast.processName);
}

void MainWindow::selectDisplay(int index)
{
    if (index < 0 || index >= m_cfg.displays.size())
        return;
    m_tabIndex = index;
    syncDisplayUi();
    refreshPreview();
}

void MainWindow::onTopologyArranged(const QVector<QPoint> &origins)
{
    if (origins.isEmpty() || m_cfg.displays.isEmpty())
        return;

    const QVector<MonitorInfo> virtuals = matchedVirtuals();
    QVector<QPair<QString, QPoint>> placements;
    for (int i = 0; i < origins.size() && i < m_cfg.displays.size(); ++i) {
        if (i >= virtuals.size() || virtuals[i].deviceName.isEmpty())
            continue;
        placements.append(qMakePair(virtuals[i].deviceName, origins[i]));
    }
    if (placements.isEmpty()) {
        m_title->setStatusHint(QStringLiteral("拓扑未应用：虚拟屏尚未出现在系统中"));
        return;
    }

    m_title->setStatusHint(QStringLiteral("正在应用多屏拓扑…"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (!WinDisplay::arrangePositions(placements)) {
        AppAlertDialog::warning(this, QStringLiteral("拓扑排列"),
                                QStringLiteral("无法写入系统显示器位置（可能需要管理员权限，或驱动暂不支持）。"));
        syncDisplayUi();
        return;
    }

    m_title->setStatusHint(QStringLiteral("已应用多屏拓扑排列"));
    QTimer::singleShot(300, this, [this]() {
        syncDisplayUi();
        refreshPreview();
    });
}

void MainWindow::updateDriverUi()
{
    const bool ready = m_vdd->driverReady();
    m_title->setDriverReady(ready, m_vdd->trackedCount());
    if (!ready) {
        m_settings->setDriverHint(m_vdd->installDriverHint());
        return;
    }
    m_settings->setDriverHint(QString());
    if (!m_cfg.displays.isEmpty()) {
        const DisplaySpec &spec = m_cfg.displays[m_tabIndex];
        m_title->setStatusHint(QStringLiteral("%1 · %2×%3 · 缩放 %4%")
                                   .arg(spec.label)
                                   .arg(spec.width)
                                   .arg(spec.height)
                                   .arg(spec.scale));
    }
}

void MainWindow::setBusyUi(bool busy)
{
    m_title->setBusy(busy);
    m_toolbar->setEnabled(!busy);
    m_canvas->setEnabled(!busy);
    if (m_clearBtn)
        m_clearBtn->setEnabled(!busy);
}

void MainWindow::openDriverPage()
{
    const QString setup = bundledDriverInstaller();
    if (!setup.isEmpty()) {
        onInstallDriver();
        return;
    }
    QDesktopServices::openUrl(QUrl(QLatin1String(kDriverReleasesUrl)));
}

QString MainWindow::bundledDriverInstaller() const
{
    const QDir app(QCoreApplication::applicationDirPath());
    const QStringList cands = {
        app.filePath(QStringLiteral("parsec-vdd/parsec-vdd-0.45.0.0.exe")),
        app.filePath(QStringLiteral("../vendor/parsec-vdd/parsec-vdd-0.45.0.0.exe")),
    };
    for (const QString &p : cands) {
        if (QFileInfo::exists(p))
            return QFileInfo(p).absoluteFilePath();
    }
    return {};
}

void MainWindow::onInstallDriver()
{
    const QString setup = bundledDriverInstaller();
    if (setup.isEmpty()) {
        QDesktopServices::openUrl(QUrl(QLatin1String(kDriverReleasesUrl)));
        AppAlertDialog::information(this, QStringLiteral("未找到捆绑驱动"),
                                 QStringLiteral("已打开网页下载 Parsec VDD。"));
        return;
    }
    if (!Elevate::isAdmin()) {
        if (!confirmElevate(QStringLiteral("安装驱动")))
            return;
        if (!Elevate::relaunchAsAdmin({QStringLiteral("--install-driver")}))
            AppAlertDialog::warning(this, QStringLiteral("提权"), QStringLiteral("无法弹出 UAC。"));
        else
            close();
        return;
    }

    const int code = QProcess::execute(setup, {QStringLiteral("/S")});
    QThread::msleep(1500);
    updateDriverUi();
    if (m_vdd->driverReady()) {
        AppAlertDialog::information(this, QStringLiteral("安装完成"),
                                 QStringLiteral("已检测到 Parsec Virtual Display Adapter。"));
    } else {
        AppAlertDialog::warning(this, QStringLiteral("安装异常"),
                             QStringLiteral("安装器退出码 %1，尚未检测到驱动。").arg(code));
    }
}

void MainWindow::showPresetHub()
{
    PresetHubDialog dlg(m_cfg, m_activeProfilePath, this);
    dlg.exec();
    if (dlg.profilesChanged()) {
        if (!m_activeProfilePath.isEmpty() && !QFile::exists(m_activeProfilePath))
            m_activeProfilePath.clear();
        rebuildSchemeCombo();
    }
    if (dlg.result() != QDialog::Accepted)
        return;
    if (dlg.action() == PresetHubDialog::LoadProfile)
        onLoadProfile(dlg.selectedPath());
    else if (dlg.action() == PresetHubDialog::SaveAsNew)
        onSaveProfileAs();
}

void MainWindow::onSchemeComboChanged(int index)
{
    if (index <= 0)
        return;
    const QString path = m_schemeCombo->itemData(index).toString();
    if (path.isEmpty())
        return;
    onLoadProfile(path);
}

void MainWindow::onSaveProfileAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("另存方案"),
        QDir(profilesDir()).filePath(QStringLiteral("双屏方案.json")),
        QStringLiteral("配置 (*.json)"));
    if (path.isEmpty())
        return;
    AppConfig c = m_cfg;
    c.profileName = QFileInfo(path).completeBaseName();
    QString err;
    if (!c.saveToFile(path, &err)) {
        AppAlertDialog::warning(this, QStringLiteral("保存失败"), err);
        return;
    }
    m_cfg = c;
    m_activeProfilePath = path;
    persistCfg();
    rebuildSchemeCombo();
    m_title->setStatusHint(QStringLiteral("已另存「%1」").arg(c.profileName));
}

void MainWindow::onLoadProfile(const QString &path)
{
    if (path.isEmpty())
        return;
    QString err;
    AppConfig c = AppConfig::loadFromFile(path, &err);
    if (!err.isEmpty() && c.displays.isEmpty()) {
        AppAlertDialog::warning(this, QStringLiteral("加载失败"), err);
        return;
    }
    const QStringList errs = c.validate();
    if (!errs.isEmpty()) {
        AppAlertDialog::warning(this, QStringLiteral("配置无效"), errs.join(QLatin1Char('\n')));
        return;
    }
    m_cfg = c;
    m_activeProfilePath = path;
    persistCfg();
    if (m_timer)
        m_timer->setInterval(qBound(100, m_cfg.previewIntervalMs, 5000));
    rebuildSchemeCombo();
    m_tabIndex = 0;
    syncDisplayUi();
    onApply();
}

void MainWindow::onDeleteProfile(const QString &path)
{
    QString err;
    if (!deleteProfileFile(path, &err)) {
        AppAlertDialog::warning(this, QStringLiteral("删除失败"), err);
        return;
    }
    rebuildSchemeCombo();
}

void MainWindow::onRefreshDisplays()
{
    if (!m_cfg.displays.isEmpty() && m_vdd->trackedCount() > 0
        && (WinDisplay::hasCloneTopology()
            || [&]() {
                   const auto virtuals = matchedVirtuals();
                   MonitorInfo primary;
                   for (const MonitorInfo &m : WinDisplay::listMonitors()) {
                       if (m.primary) {
                           primary = m;
                           break;
                       }
                   }
                   for (const MonitorInfo &v : virtuals) {
                       if (v.deviceName.isEmpty())
                           continue;
                       if (v.geometry.intersects(primary.geometry)
                           && v.geometry.intersected(primary.geometry).width()
                               > v.geometry.width() / 2)
                           return true;
                   }
                   return false;
               }())) {
        const AppConfig c = m_cfg;
        runBg([this, c]() { return m_vdd->rearrange(c); }, QStringLiteral("扩展重排"));
        return;
    }
    syncDisplayUi();
    refreshPreview();
}

void MainWindow::onAddDisplay()
{
    if (!m_vdd->driverReady()) {
        if (AppAlertDialog::question(
                this, QStringLiteral("缺少驱动"),
                m_vdd->installDriverHint() + QStringLiteral("\n\n是否安装/打开驱动页？"),
                QStringLiteral("安装"), QStringLiteral("取消")))
            openDriverPage();
        return;
    }
    AddDisplayDialog dlg(m_cfg.displays.size() + 1, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    addDisplaySpec(dlg.resultSpec());
}

void MainWindow::addDisplaySpec(const DisplaySpec &spec)
{
    DisplaySpec s = spec;
    if (s.label.trimmed().isEmpty())
        s.label = QStringLiteral("屏%1").arg(m_cfg.displays.size() + 1);
    m_pendingSpec = s;
    m_pendingIndex = m_cfg.displays.size();
    runBg([this, s]() { return m_vdd->addOne(s); }, QStringLiteral("添加"));
}

void MainWindow::removeDisplayAt(int index)
{
    if (index < 0 || index >= m_cfg.displays.size())
        return;
    m_pendingIndex = index;
    const int tracked = m_vdd->trackedCount();
    runBg([this, index, tracked]() {
        if (index < tracked)
            return m_vdd->removeAt(index);
        return QStringLiteral("已删除（仅配置）。");
    }, QStringLiteral("删除"));
}

void MainWindow::updateDisplayAt(int index, const DisplaySpec &spec)
{
    if (index < 0 || index >= m_cfg.displays.size())
        return;
    m_pendingSpec = spec;
    m_pendingIndex = index;
    QVector<DisplaySpec> all = m_cfg.displays;
    all[index] = spec;
    runBg([this, index, spec, all]() {
        if (index < m_vdd->trackedCount())
            return m_vdd->updateAt(index, spec, all);
        return QStringLiteral("已更新配置。");
    }, QStringLiteral("更新"));
}

void MainWindow::onPlaceWindow(int index)
{
    if (m_cfg.displays.isEmpty())
        return;
    index = qBound(0, index, m_cfg.displays.size() - 1);
    const QVector<MonitorInfo> virtuals = matchedVirtuals();
    if (index >= virtuals.size() || virtuals[index].deviceName.isEmpty()) {
        AppAlertDialog::warning(this, QStringLiteral("投放窗口"),
                             QStringLiteral("该虚拟屏尚未出现在系统显示器列表中。"));
        return;
    }
    CastWindowDialog dlg(m_cfg.displays[index].label, virtuals[index], this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    while (m_castByDisplay.size() < m_cfg.displays.size())
        m_castByDisplay.append(DisplayCastInfo());
    m_castByDisplay[index].title = dlg.selectedTitle();
    m_castByDisplay[index].processName = dlg.selectedProcessName();
    syncDrawerCast();
    m_title->setStatusHint(QStringLiteral("已投放到「%1」").arg(m_cfg.displays[index].label));
    QTimer::singleShot(400, this, &MainWindow::refreshPreview);
}

bool MainWindow::confirmElevate(const QString &action)
{
    return AppAlertDialog::question(
        this, QStringLiteral("需要管理员权限"),
        QStringLiteral("「%1」需要管理员权限，将弹出 UAC。").arg(action),
        QStringLiteral("继续"), QStringLiteral("取消"));
}

static bool looksSuccess(const QString &title, const QString &msg)
{
    Q_UNUSED(title);
    if (msg.startsWith(QStringLiteral("已应用")) || msg.startsWith(QStringLiteral("已更新"))
        || msg.startsWith(QStringLiteral("已添加")) || msg.startsWith(QStringLiteral("已删除"))
        || msg.startsWith(QStringLiteral("已就绪")) || msg.startsWith(QStringLiteral("已请求禁用"))
        || msg.contains(QStringLiteral("未找到 Parsec")) || msg.contains(QStringLiteral("无需清除"))
        || msg.startsWith(QStringLiteral("已强制扩展")))
        return true;
    return !msg.contains(QStringLiteral("失败"));
}

void MainWindow::runBg(const std::function<QString()> &work, const QString &title)
{
    if (m_busy) {
        AppAlertDialog::information(this, QStringLiteral("请稍候"), QStringLiteral("已有任务进行中。"));
        return;
    }
    m_busy = true;
    setBusyUi(true);
    m_canvas->setPlaceholderText(title + QStringLiteral("中…"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QString msg;
    bool ok = true;
    try {
        msg = work();
    } catch (...) {
        ok = false;
        msg = QStringLiteral("未知错误");
    }
    ok = ok && looksSuccess(title, msg);

    m_busy = false;
    setBusyUi(false);
    m_title->setStatusHint(msg.split(QLatin1Char('\n')).value(0));

    if (!ok) {
        AppAlertDialog::warning(this, title, msg);
        syncDisplayUi();
        refreshPreview();
        return;
    }

    if (title == QStringLiteral("添加")) {
        m_cfg.displays.push_back(m_pendingSpec);
        m_tabIndex = m_cfg.displays.size() - 1;
    } else if (title == QStringLiteral("删除") && m_pendingIndex >= 0
               && m_pendingIndex < m_cfg.displays.size()) {
        m_cfg.displays.removeAt(m_pendingIndex);
        if (m_pendingIndex < m_castByDisplay.size())
            m_castByDisplay.removeAt(m_pendingIndex);
        if (m_tabIndex >= m_cfg.displays.size())
            m_tabIndex = qMax(0, m_cfg.displays.size() - 1);
    } else if (title == QStringLiteral("更新") && m_pendingIndex >= 0
               && m_pendingIndex < m_cfg.displays.size()) {
        m_cfg.displays[m_pendingIndex] = m_pendingSpec;
    } else if (title == QStringLiteral("清除")) {
        m_cfg.displays.clear();
        m_castByDisplay.clear();
        m_tabIndex = 0;
    }

    persistCfg();
    syncDisplayUi();
    updateDriverUi();
    if (msg.contains(QStringLiteral("警告")) || msg.contains(QStringLiteral("提示：")))
        AppAlertDialog::information(this, title, msg);

    QTimer::singleShot(400, this, &MainWindow::refreshPreview);
}

void MainWindow::onApply()
{
    const QStringList errs = m_cfg.validate();
    if (!errs.isEmpty()) {
        AppAlertDialog::warning(this, QStringLiteral("配置错误"), errs.join(QLatin1Char('\n')));
        return;
    }
    if (!m_vdd->driverReady()) {
        if (AppAlertDialog::question(this, QStringLiteral("缺少驱动"), m_vdd->installDriverHint(),
                                       QStringLiteral("安装"), QStringLiteral("取消")))
            openDriverPage();
        return;
    }
    persistCfg();
    const AppConfig c = m_cfg;
    runBg([this, c]() { return m_vdd->applyConfig(c); }, QStringLiteral("应用"));
}

void MainWindow::onClear()
{
    if (!AppAlertDialog::question(this, QStringLiteral("清除全部"), QStringLiteral("移除所有虚拟屏？"),
                                    QStringLiteral("清除"), QStringLiteral("取消")))
        return;
    runBg([this]() { return m_vdd->clearVirtualDisplays(); }, QStringLiteral("清除"));
}

QVector<MonitorInfo> MainWindow::matchedVirtuals() const
{
    QVector<MonitorInfo> pool;
    for (const MonitorInfo &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            pool.push_back(m);
    }
    QVector<MonitorInfo> out(m_cfg.displays.size());
    for (int i = 0; i < m_cfg.displays.size() && i < pool.size(); ++i)
        out[i] = pool[i];
    return out;
}

void MainWindow::capturePreviewForIndex(int index, const QSize &targetSize)
{
    const QVector<MonitorInfo> virtuals = matchedVirtuals();
    if (index < 0 || index >= virtuals.size() || virtuals[index].deviceName.isEmpty())
        return;

    const MonitorInfo mon = virtuals[index];
    const qreal dpr = qMax<qreal>(1.0, m_canvas->devicePixelRatioF());
    auto *th = QThread::create([this, mon, targetSize, index, dpr]() {
        QImage img = WinDisplay::captureDesktopRect(mon.geometry);
        QPixmap pm;
        if (!img.isNull()) {
            if (targetSize.width() > 1 && targetSize.height() > 1)
                img = img.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            pm = QPixmap::fromImage(img);
            if (dpr > 1.01)
                pm.setDevicePixelRatio(dpr);
        }
        QMetaObject::invokeMethod(this, [this, pm, index]() {
            m_canvas->setPreviewPixmap(index, pm);
            if (m_pendingGrabs > 0) {
                --m_pendingGrabs;
                if (m_pendingGrabs <= 0)
                    m_grabBusy = false;
            } else {
                m_grabBusy = false;
            }
        }, Qt::QueuedConnection);
    });
    connect(th, &QThread::finished, th, &QObject::deleteLater);
    th->start();
}

void MainWindow::refreshPreview()
{
    if (m_grabBusy || isMinimized() || !isVisible() || m_busy)
        return;
    if (m_cfg.displays.isEmpty()) {
        m_canvas->setPlaceholderText(QStringLiteral("点「+ 添加显示」或从方案预设加载"));
        return;
    }

    m_tabIndex = qBound(0, m_tabIndex, m_cfg.displays.size() - 1);

    const TopologyCanvas::ViewMode mode = m_canvas->viewMode();
    if (mode == TopologyCanvas::Topology) {
        m_canvas->update();
        return;
    }

    m_grabBusy = true;
    const qreal dpr = qMax<qreal>(1.0, m_canvas->devicePixelRatioF());
    const int tab = m_tabIndex;

    if (mode == TopologyCanvas::Focus) {
        const QSize target(qRound(m_canvas->width() * dpr), qRound(m_canvas->height() * dpr));
        capturePreviewForIndex(tab, target);
        return;
    }

    // 全景网格：多屏抓取，全部完成后再允许下一轮
    const int n = m_cfg.displays.size();
    m_pendingGrabs = n;
    for (int i = 0; i < n; ++i) {
        const QSize logical = m_canvas->gridPreviewSize(i);
        const QSize target(qMax(2, qRound(logical.width() * dpr)),
                           qMax(2, qRound(logical.height() * dpr)));
        capturePreviewForIndex(i, target);
    }
}

void MainWindow::showFocusFullscreen()
{
    if (m_cfg.displays.isEmpty())
        return;

    m_tabIndex = qBound(0, m_tabIndex, m_cfg.displays.size() - 1);
    QPixmap pm = m_canvas->previewPixmap(m_tabIndex);
    if (pm.isNull()) {
        const QVector<MonitorInfo> virtuals = matchedVirtuals();
        if (m_tabIndex < virtuals.size() && !virtuals[m_tabIndex].deviceName.isEmpty())
            pm = QPixmap::fromImage(WinDisplay::captureDesktopRect(virtuals[m_tabIndex].geometry));
    }
    if (pm.isNull()) {
        AppAlertDialog::information(this, QStringLiteral("全屏预览"),
                                    QStringLiteral("当前屏尚无预览画面，请稍候再试。"));
        return;
    }

    class FullscreenPreview : public QDialog
    {
    public:
        explicit FullscreenPreview(const QPixmap &pm, QWidget *parent = nullptr)
            : QDialog(parent)
        {
            setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
            setAttribute(Qt::WA_DeleteOnClose, true);
            setStyleSheet(QStringLiteral("background:#000000;"));

            auto *lay = new QVBoxLayout(this);
            lay->setContentsMargins(0, 0, 0, 0);
            lay->setSpacing(0);

            auto *lab = new QLabel(this);
            lab->setAlignment(Qt::AlignCenter);
            QScreen *screen = QGuiApplication::primaryScreen();
            const QSize sz = screen ? screen->size() : QSize(1920, 1080);
            lab->setPixmap(pm.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            lay->addWidget(lab, 1);

            auto *hint = new QLabel(QStringLiteral("按 Esc 或点击画面退出全屏"), this);
            hint->setAlignment(Qt::AlignCenter);
            hint->setStyleSheet(
                QStringLiteral("color:#94a3b8;font-size:12px;padding:12px;background:transparent;"));
            lay->addWidget(hint);
        }

    protected:
        void keyPressEvent(QKeyEvent *event) override
        {
            if (event->key() == Qt::Key_Escape) {
                accept();
                return;
            }
            QDialog::keyPressEvent(event);
        }

        void mousePressEvent(QMouseEvent *event) override
        {
            Q_UNUSED(event);
            accept();
        }
    };

    auto *dlg = new FullscreenPreview(pm, this);
    dlg->showFullScreen();
}

void MainWindow::saveFocusScreenshot()
{
    if (m_cfg.displays.isEmpty())
        return;

    m_tabIndex = qBound(0, m_tabIndex, m_cfg.displays.size() - 1);
    QPixmap pm = m_canvas->previewPixmap(m_tabIndex);
    if (pm.isNull()) {
        const QVector<MonitorInfo> virtuals = matchedVirtuals();
        if (m_tabIndex < virtuals.size() && !virtuals[m_tabIndex].deviceName.isEmpty())
            pm = QPixmap::fromImage(WinDisplay::captureDesktopRect(virtuals[m_tabIndex].geometry));
    }
    if (pm.isNull()) {
        AppAlertDialog::information(this, QStringLiteral("截图"),
                                    QStringLiteral("当前屏尚无预览画面，请稍候再试。"));
        return;
    }

    const DisplaySpec &spec = m_cfg.displays[m_tabIndex];
    const QString suggest = QStringLiteral("%1_%2x%3.png")
                                .arg(spec.label)
                                .arg(spec.width)
                                .arg(spec.height);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存虚拟屏截图"), suggest,
        QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg)"));
    if (path.isEmpty())
        return;

    if (!pm.save(path)) {
        AppAlertDialog::warning(this, QStringLiteral("截图"),
                                QStringLiteral("保存失败：%1").arg(path));
        return;
    }
    m_title->setStatusHint(QStringLiteral("截图已保存：%1").arg(QFileInfo(path).fileName()));
}
