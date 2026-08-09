#include "MainWindow.h"

#include "Elevate.h"
#include "PreviewPane.h"
#include "SettingsPanel.h"
#include "TitleBar.h"
#include "VddService.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
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

static QString btnStyle(bool primary)
{
    if (primary) {
        return QStringLiteral(
            "QPushButton { color:#0B1220; background:#F8FAFC; border:1px solid #F8FAFC;"
            " padding:6px 14px; font-weight:600; }"
            "QPushButton:hover { background:#E2E8F0; }"
            "QPushButton:disabled { color:#64748B; background:#1E293B; border-color:#334155; }");
    }
    return QStringLiteral(
        "QPushButton { color:#E2E8F0; background:transparent; border:1px solid #64748B;"
        " padding:6px 12px; }"
        "QPushButton:hover { background:#1E293B; border-color:#94A3B8; }"
        "QPushButton:disabled { color:#64748B; border-color:#334155; }");
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setMinimumSize(960, 640);
    resize(1280, 800);
    setStyleSheet(QStringLiteral("MainWindow { background:#0B1220; }"));

    m_cfg = AppConfig::load();
    m_vdd = new VddService(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_title = new TitleBar(this);
    m_title->setAdminHint(Elevate::isAdmin() ? QStringLiteral("管理员") : QStringLiteral("普通权限"));
    root->addWidget(m_title);

    m_tabBar = new QWidget(this);
    m_tabBar->setFixedHeight(36);
    m_tabBar->setStyleSheet(QStringLiteral("background:#0B1220;"));
    m_tabLay = new QHBoxLayout(m_tabBar);
    m_tabLay->setContentsMargins(10, 4, 10, 4);
    m_tabLay->setSpacing(6);
    m_tabLay->addStretch();
    m_previewToggle = new QPushButton(QStringLiteral("预览:关"), m_tabBar);
    m_previewToggle->setCursor(Qt::PointingHandCursor);
    m_previewToggle->setStyleSheet(btnStyle(false));
    m_tabLay->addWidget(m_previewToggle);
    root->addWidget(m_tabBar);

    m_preview = new PreviewPane(this);
    root->addWidget(m_preview, 1);

    m_bottom = new QWidget(this);
    m_bottom->setFixedHeight(52);
    m_bottom->setStyleSheet(QStringLiteral("background:#111827; border-top:1px solid #1E293B;"));
    auto *bot = new QHBoxLayout(m_bottom);
    bot->setContentsMargins(12, 8, 12, 8);
    bot->setSpacing(8);

    auto *brand = new QLabel(QStringLiteral("VirtualScreen"), m_bottom);
    brand->setStyleSheet(QStringLiteral("color:#94A3B8; font-weight:600;"));
    bot->addWidget(brand);
    bot->addStretch();

    auto *profileBtn = new QPushButton(QStringLiteral("方案…"), m_bottom);
    auto *refreshBtn = new QPushButton(QStringLiteral("刷新"), m_bottom);
    auto *placeBtn = new QPushButton(QStringLiteral("投放窗口"), m_bottom);
    auto *customBtn = new QPushButton(QStringLiteral("自定义…"), m_bottom);
    auto *addBtn = new QPushButton(QStringLiteral("添加显示"), m_bottom);
    profileBtn->setStyleSheet(btnStyle(false));
    refreshBtn->setStyleSheet(btnStyle(false));
    placeBtn->setStyleSheet(btnStyle(false));
    customBtn->setStyleSheet(btnStyle(false));
    addBtn->setStyleSheet(btnStyle(true));
    for (QPushButton *b : {profileBtn, refreshBtn, placeBtn, customBtn, addBtn})
        b->setCursor(Qt::PointingHandCursor);
    bot->addWidget(profileBtn);
    bot->addWidget(refreshBtn);
    bot->addWidget(placeBtn);
    bot->addWidget(customBtn);
    bot->addWidget(addBtn);
    root->addWidget(m_bottom);

    m_settings = new SettingsDialog(this);
    m_settings->loadFrom(m_cfg);

    connect(m_title, &TitleBar::clearClicked, this, &MainWindow::onClear);
    connect(m_title, &TitleBar::closeClicked, this, &QWidget::close);
    connect(m_previewToggle, &QPushButton::clicked, this, &MainWindow::togglePreview);
    connect(profileBtn, &QPushButton::clicked, this, &MainWindow::showProfileMenu);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshDisplays);
    connect(placeBtn, &QPushButton::clicked, this, &MainWindow::onPlaceWindow);
    connect(customBtn, &QPushButton::clicked, this, &MainWindow::onCustomDialog);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddDisplay);
    connect(m_preview, &PreviewPane::primaryClicked, this, &MainWindow::onGuidePrimary);
    connect(m_preview, &PreviewPane::secondaryClicked, this, &MainWindow::onGuideSecondary);
    connect(m_vdd, &VddService::progress, this, [this](const QString &m) {
        m_title->setStatusHint(m);
    });
    connect(m_settings, &SettingsDialog::saveRequested, this, [this]() {
        AppConfig c = m_settings->toConfig(m_cfg);
        const QStringList errs = c.validate();
        if (!errs.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("配置错误"), errs.join(QLatin1Char('\n')));
            return;
        }
        m_cfg = c;
        persistCfg();
        rebuildTabs();
        m_title->setStatusHint(QStringLiteral("方案已保存 · 正在应用…"));
        onApply();
    });
    connect(m_settings, &SettingsDialog::saveAsRequested, this, &MainWindow::onSaveProfileAs);
    connect(m_settings, &SettingsDialog::loadProfileRequested, this, &MainWindow::onLoadProfile);
    connect(m_settings, &SettingsDialog::browseLoadRequested, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("加载配置"), profilesDir(), QStringLiteral("配置 (*.json)"));
        if (!path.isEmpty())
            onLoadProfile(path);
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshPreview);
    m_timer->start(qMax(1500, m_cfg.previewIntervalMs));

    rebuildTabs();
    if (m_vdd->driverReady()) {
        if (m_cfg.displays.isEmpty()) {
            m_title->setStatusHint(QStringLiteral("驱动就绪 · 点「添加显示」"));
            refreshGuide();
        } else {
            m_title->setStatusHint(QStringLiteral("正在挂上方案「%1」…")
                                       .arg(m_cfg.profileName.isEmpty() ? QStringLiteral("当前")
                                                                       : m_cfg.profileName));
            // 配置里有屏但进程重启后 ping 已断，自动按方案挂回
            QTimer::singleShot(400, this, &MainWindow::onApply);
        }
    } else {
        updateDriverUi();
        refreshGuide();
    }
}

void MainWindow::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange && m_title)
        m_title->syncMaxButton();
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
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
    s.label = QStringLiteral("虚拟屏%1").arg(ordinal);
    s.width = 1920;
    s.height = 1080;
    s.hz = 60;
    s.scale = 100;
    return s;
}

void MainWindow::updateDriverUi()
{
    if (m_vdd->driverReady()) {
        m_settings->setDriverHint(QString());
        return;
    }
    m_title->setStatusHint(QStringLiteral("未检测到驱动"));
    m_settings->setDriverHint(m_vdd->installDriverHint());
}

void MainWindow::setPreviewEnabled(bool on)
{
    if (m_previewOn == on) {
        if (on)
            refreshPreview();
        else
            refreshGuide();
        return;
    }
    m_previewOn = on;
    m_previewToggle->setText(m_previewOn ? QStringLiteral("预览:开") : QStringLiteral("预览:关"));
    m_previewToggle->setStyleSheet(m_previewOn ? btnStyle(true) : btnStyle(false));
    if (m_previewOn)
        refreshPreview();
    else
        refreshGuide();
}

void MainWindow::setBusyUi(bool busy)
{
    m_title->setBusy(busy);
    m_previewToggle->setEnabled(!busy);
    m_bottom->setEnabled(!busy);
    for (QPushButton *b : m_tabs)
        b->setEnabled(!busy);
}

void MainWindow::openDriverPage()
{
    const QString setup = bundledDriverInstaller();
    if (!setup.isEmpty()) {
        onInstallDriver();
        return;
    }
    QDesktopServices::openUrl(QUrl(QLatin1String(kDriverReleasesUrl)));
    m_title->setStatusHint(QStringLiteral("已打开驱动下载页"));
}

QString MainWindow::bundledDriverInstaller() const
{
    const QDir app(QCoreApplication::applicationDirPath());
    const QStringList cands = {
        app.filePath(QStringLiteral("parsec-vdd/parsec-vdd-0.45.0.0.exe")),
        app.filePath(QStringLiteral("../vendor/parsec-vdd/parsec-vdd-0.45.0.0.exe")),
        app.filePath(QStringLiteral("../../vendor/parsec-vdd/parsec-vdd-0.45.0.0.exe")),
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
        QMessageBox::information(
            this,
            QStringLiteral("未找到捆绑驱动"),
            QStringLiteral("本目录没有 parsec-vdd\\parsec-vdd-0.45.0.0.exe。\n"
                           "已打开网页；也可从仓库 vendor\\parsec-vdd 拷到 exe 旁。"));
        return;
    }
    if (!Elevate::isAdmin()) {
        if (!confirmElevate(QStringLiteral("安装驱动")))
            return;
        if (!Elevate::relaunchAsAdmin({QStringLiteral("--install-driver")}))
            QMessageBox::warning(this, QStringLiteral("提权"), QStringLiteral("无法弹出 UAC，或已取消。"));
        else
            close();
        return;
    }

    m_title->setStatusHint(QStringLiteral("正在安装 Parsec VDD…"));
    const int code = QProcess::execute(setup, {QStringLiteral("/S")});
    QThread::msleep(1500);
    updateDriverUi();
    if (m_vdd->driverReady()) {
        m_title->setStatusHint(QStringLiteral("驱动安装完成"));
        QMessageBox::information(this, QStringLiteral("安装完成"),
                                 QStringLiteral("已检测到 Parsec Virtual Display Adapter。\n可以点「添加显示」了。"));
        refreshGuide();
    } else {
        m_title->setStatusHint(QStringLiteral("安装结束，未检测到驱动"));
        QMessageBox::warning(
            this,
            QStringLiteral("安装异常"),
            QStringLiteral("安装器已运行（退出码 %1），但尚未检测到适配器。\n"
                           "请打开设备管理器查看，或手动再跑一次：\n%2")
                .arg(code)
                .arg(setup));
        refreshGuide();
    }
}

void MainWindow::refreshGuide()
{
    if (m_previewOn || m_busy)
        return;

    if (!m_vdd->driverReady()) {
        const QString setup = bundledDriverInstaller();
        m_preview->setGuide(
            QStringLiteral("还差一步：安装 Parsec 虚拟显示驱动"),
            setup.isEmpty()
                ? QStringLiteral(
                      "1. 点下方打开下载页，安装 Parsec VDD\n"
                      "2. 装好后设备里应出现 Parsec Virtual Display Adapter\n"
                      "3. 回到本程序，点「添加显示」")
                : QStringLiteral(
                      "本程序已捆绑驱动安装包。\n"
                      "1. 点下方「安装捆绑驱动」（会弹 UAC）\n"
                      "2. 装好后点「添加显示」\n"
                      "（建议不要同时开官方 ParsecVDisplay）"),
            setup.isEmpty() ? QStringLiteral("打开驱动下载页") : QStringLiteral("安装捆绑驱动"),
            QStringLiteral("查看安装说明"));
        return;
    }

    if (m_vdd->trackedCount() == 0) {
        if (!m_cfg.displays.isEmpty()) {
            m_preview->setGuide(
                QStringLiteral("方案已加载，尚未挂上虚拟屏"),
                QStringLiteral(
                    "当前方案「%1」含 %2 块屏。\n"
                    "点下方应用即可按方案创建；也可清空后逐个「添加显示」。\n"
                    "挂上后右键标签可改分辨率 / 刷新率 / 缩放 / 删除。")
                    .arg(m_cfg.profileName.isEmpty() ? QStringLiteral("当前") : m_cfg.profileName)
                    .arg(m_cfg.displays.size()),
                QStringLiteral("应用方案"),
                QStringLiteral("添加显示"));
            return;
        }
        m_preview->setGuide(
            QStringLiteral("先添加一块虚拟屏！"),
            QStringLiteral(
                "1. 点右下角「添加显示」立刻挂上一块虚拟屏\n"
                "2. 在上方标签右键：改分辨率 / 刷新率 / 缩放 / 删除\n"
                "3. 可用「方案…」保存、加载、删除配置；「投放窗口」把应用挪到虚拟屏\n"
                "4. 打开「预览」查看虚拟屏画面"),
            QStringLiteral("添加显示"),
            QStringLiteral("打开预览"));
        return;
    }

    const DisplaySpec &s = m_cfg.displays[qBound(0, m_tabIndex, m_cfg.displays.size() - 1)];
    m_preview->setGuide(
        QStringLiteral("虚拟屏已就绪"),
        QStringLiteral("当前：%1  %2×%3 @%4Hz  缩放%5%\n"
                       "右键标签可改规格；点「预览」看画面；「投放窗口」移动应用。")
            .arg(s.label)
            .arg(s.width)
            .arg(s.height)
            .arg(s.hz)
            .arg(s.scale),
        QStringLiteral("打开预览"),
        QStringLiteral("投放窗口"));
}

void MainWindow::onGuidePrimary()
{
    if (!m_vdd->driverReady()) {
        if (!bundledDriverInstaller().isEmpty())
            onInstallDriver();
        else
            openDriverPage();
        return;
    }
    if (m_vdd->trackedCount() == 0) {
        if (!m_cfg.displays.isEmpty())
            onApply();
        else
            onAddDisplay();
        return;
    }
    setPreviewEnabled(true);
}

void MainWindow::onGuideSecondary()
{
    if (!m_vdd->driverReady()) {
        QMessageBox::information(this, QStringLiteral("安装说明"), m_vdd->installDriverHint());
        return;
    }
    if (m_vdd->trackedCount() == 0) {
        if (!m_cfg.displays.isEmpty())
            onAddDisplay();
        else
            setPreviewEnabled(true);
        return;
    }
    onPlaceWindow();
}

void MainWindow::rebuildTabs()
{
    for (QPushButton *b : m_tabs) {
        m_tabLay->removeWidget(b);
        b->deleteLater();
    }
    m_tabs.clear();

    // stretch 在索引 0，预览按钮在末尾 → 插在 stretch 之后
    for (int i = 0; i < m_cfg.displays.size(); ++i) {
        const DisplaySpec &spec = m_cfg.displays[i];
        const QString text = spec.label.trimmed().isEmpty()
                                 ? QStringLiteral("虚拟屏%1").arg(i + 1)
                                 : spec.label.trimmed();
        auto *btn = new QPushButton(text, m_tabBar);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setEnabled(!m_busy);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        const bool active = (i == m_tabIndex);
        btn->setStyleSheet(active
            ? QStringLiteral(
                  "QPushButton { color:#fff; background:#0F766E; padding:4px 14px; border:none; }"
                  "QPushButton:hover { background:#0D9488; }")
            : QStringLiteral(
                  "QPushButton { color:#CBD5E1; background:#1E293B; padding:4px 14px; border:1px solid #334155; }"
                  "QPushButton:hover { background:#334155; }"));
        btn->setToolTip(QStringLiteral("%1  %2×%3 @%4Hz  缩放%5%\n右键可改规格 / 删除")
                            .arg(text)
                            .arg(spec.width)
                            .arg(spec.height)
                            .arg(spec.hz)
                            .arg(spec.scale));
        m_tabLay->insertWidget(i, btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { selectTab(i); });
        connect(btn, &QWidget::customContextMenuRequested, this, [this, i, btn](const QPoint &p) {
            showDisplayContextMenu(i, btn->mapToGlobal(p));
        });
        m_tabs.push_back(btn);
    }
    if (m_tabIndex >= m_cfg.displays.size())
        m_tabIndex = qMax(0, m_cfg.displays.size() - 1);
}

void MainWindow::selectTab(int index)
{
    m_tabIndex = index;
    rebuildTabs();
    if (m_previewOn)
        refreshPreview();
    else
        refreshGuide();
}

void MainWindow::togglePreview()
{
    setPreviewEnabled(!m_previewOn);
}

void MainWindow::showDisplayContextMenu(int index, const QPoint &globalPos)
{
    if (index < 0 || index >= m_cfg.displays.size())
        return;
    selectTab(index);
    DisplaySpec spec = m_cfg.displays[index];

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background:#111827; color:#E2E8F0; border:1px solid #334155; }"
        "QMenu::item:selected { background:#0F766E; }"));

    QMenu *res = menu.addMenu(QStringLiteral("分辨率"));
    const QList<QPair<int, int>> presets = {
        {1920, 1080}, {1920, 1200}, {2560, 1440}, {3840, 2160},
        {1280, 720}, {1366, 768}, {1600, 900}, {1680, 1050},
    };
    for (const auto &wh : presets) {
        auto *a = res->addAction(QStringLiteral("%1 × %2").arg(wh.first).arg(wh.second));
        connect(a, &QAction::triggered, this, [this, index, wh]() {
            DisplaySpec s = m_cfg.displays[index];
            s.width = wh.first;
            s.height = wh.second;
            updateDisplayAt(index, s);
        });
    }
    res->addSeparator();
    connect(res->addAction(QStringLiteral("自定义…")), &QAction::triggered, this, [this, index]() {
        DisplaySpec s = m_cfg.displays[index];
        bool ok = false;
        const int w = QInputDialog::getInt(this, QStringLiteral("宽度"), QStringLiteral("像素宽"),
                                           s.width, 640, 7680, 8, &ok);
        if (!ok)
            return;
        const int h = QInputDialog::getInt(this, QStringLiteral("高度"), QStringLiteral("像素高"),
                                           s.height, 480, 4320, 8, &ok);
        if (!ok)
            return;
        s.width = w;
        s.height = h;
        updateDisplayAt(index, s);
    });

    QMenu *hz = menu.addMenu(QStringLiteral("刷新率"));
    for (int v : {30, 60, 90, 120, 144}) {
        auto *a = hz->addAction(QStringLiteral("%1 Hz").arg(v));
        connect(a, &QAction::triggered, this, [this, index, v]() {
            DisplaySpec s = m_cfg.displays[index];
            s.hz = v;
            updateDisplayAt(index, s);
        });
    }

    QMenu *scale = menu.addMenu(QStringLiteral("缩放"));
    for (int v : {100, 125, 150, 175, 200}) {
        auto *a = scale->addAction(QStringLiteral("%1%").arg(v));
        connect(a, &QAction::triggered, this, [this, index, v]() {
            DisplaySpec s = m_cfg.displays[index];
            s.scale = v;
            updateDisplayAt(index, s);
        });
    }

    menu.addSeparator();
    connect(menu.addAction(QStringLiteral("投放窗口到此屏…")), &QAction::triggered, this, [this]() {
        onPlaceWindow();
    });
    menu.addSeparator();
    connect(menu.addAction(QStringLiteral("删除此虚拟屏")), &QAction::triggered, this, [this, index]() {
        removeDisplayAt(index);
    });

    menu.exec(globalPos);
}

void MainWindow::showProfileMenu()
{
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background:#111827; color:#E2E8F0; border:1px solid #334155; }"
        "QMenu::item:selected { background:#0F766E; }"));
    connect(menu.addAction(QStringLiteral("保存当前方案")), &QAction::triggered,
            this, &MainWindow::onSaveProfile);
    connect(menu.addAction(QStringLiteral("另存为…")), &QAction::triggered,
            this, &MainWindow::onSaveProfileAs);

    QMenu *load = menu.addMenu(QStringLiteral("加载方案"));
    const QStringList paths = listProfilePaths();
    if (paths.isEmpty()) {
        auto *empty = load->addAction(QStringLiteral("（暂无已存方案）"));
        empty->setEnabled(false);
    } else {
        for (const QString &p : paths) {
            auto *a = load->addAction(QFileInfo(p).completeBaseName());
            connect(a, &QAction::triggered, this, [this, p]() { onLoadProfile(p); });
        }
    }
    connect(menu.addAction(QStringLiteral("浏览加载…")), &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("加载配置"), profilesDir(), QStringLiteral("配置 (*.json)"));
        if (!path.isEmpty())
            onLoadProfile(path);
    });

    QMenu *del = menu.addMenu(QStringLiteral("删除方案"));
    if (paths.isEmpty()) {
        auto *empty = del->addAction(QStringLiteral("（暂无）"));
        empty->setEnabled(false);
    } else {
        for (const QString &p : paths) {
            auto *a = del->addAction(QFileInfo(p).completeBaseName());
            connect(a, &QAction::triggered, this, [this, p]() { onDeleteProfile(p); });
        }
    }
    menu.exec(QCursor::pos());
}

void MainWindow::onSaveProfile()
{
    if (m_cfg.profileName.isEmpty()) {
        onSaveProfileAs();
        return;
    }
    const QString path = QDir(profilesDir()).filePath(m_cfg.profileName + QStringLiteral(".json"));
    QString err;
    if (!m_cfg.saveToFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), err);
        return;
    }
    persistCfg();
    m_title->setStatusHint(QStringLiteral("已保存方案「%1」").arg(m_cfg.profileName));
}

void MainWindow::onSaveProfileAs()
{
    AppConfig c = m_settings->isVisible() ? m_settings->toConfig(m_cfg) : m_cfg;
    const QStringList errs = c.validate();
    if (!errs.isEmpty() && !c.displays.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置错误"), errs.join(QLatin1Char('\n')));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("另存方案"),
        QDir(profilesDir()).filePath(QStringLiteral("project.json")),
        QStringLiteral("配置 (*.json)"));
    if (path.isEmpty())
        return;
    c.profileName = QFileInfo(path).completeBaseName();
    QString err;
    if (!c.saveToFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), err);
        return;
    }
    m_cfg = c;
    persistCfg();
    rebuildTabs();
    m_title->setStatusHint(QStringLiteral("已另存「%1」").arg(c.profileName));
}

void MainWindow::onLoadProfile(const QString &path)
{
    if (path.isEmpty())
        return;
    QString err;
    AppConfig c = AppConfig::loadFromFile(path, &err);
    if (!err.isEmpty() && c.displays.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), err);
        return;
    }
    const QStringList errs = c.validate();
    if (!errs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置无效"), errs.join(QLatin1Char('\n')));
        return;
    }
    m_cfg = c;
    persistCfg();
    rebuildTabs();
    m_title->setStatusHint(QStringLiteral("已加载「%1」· 正在应用…").arg(m_cfg.profileName));
    onApply();
}

void MainWindow::onDeleteProfile(const QString &path)
{
    if (QMessageBox::question(this, QStringLiteral("删除方案"),
                              QStringLiteral("删除「%1」？\n不会影响当前已挂上的虚拟屏。")
                                  .arg(QFileInfo(path).completeBaseName()))
        != QMessageBox::Yes)
        return;
    QString err;
    if (!deleteProfileFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), err);
        return;
    }
    m_title->setStatusHint(QStringLiteral("已删除方案「%1」").arg(QFileInfo(path).completeBaseName()));
}

void MainWindow::onCustomDialog()
{
    updateDriverUi();
    m_settings->loadFrom(m_cfg);
    m_settings->exec();
    rebuildTabs();
    if (!m_previewOn)
        refreshGuide();
}

void MainWindow::onRefreshDisplays()
{
    rebuildTabs();
    if (m_previewOn)
        refreshPreview();
    else
        refreshGuide();
    m_title->setStatusHint(QStringLiteral("已刷新 · 虚拟屏 %1 / 配置 %2")
                               .arg(m_vdd->trackedCount())
                               .arg(m_cfg.displays.size()));
}

void MainWindow::onAddDisplay()
{
    if (!m_vdd->driverReady()) {
        const auto ans = QMessageBox::question(
            this, QStringLiteral("缺少驱动"),
            m_vdd->installDriverHint() + QStringLiteral("\n\n是否安装/打开驱动页？"));
        if (ans == QMessageBox::Yes)
            openDriverPage();
        refreshGuide();
        return;
    }
    DisplaySpec spec = defaultSpec(m_cfg.displays.size() + 1);
    addDisplaySpec(spec);
}

void MainWindow::addDisplaySpec(const DisplaySpec &spec)
{
    DisplaySpec s = spec;
    if (s.label.trimmed().isEmpty())
        s.label = QStringLiteral("虚拟屏%1").arg(m_cfg.displays.size() + 1);

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
        return QStringLiteral("已更新配置（驱动侧尚未挂上该屏，请先添加）。");
    }, QStringLiteral("更新"));
}

void MainWindow::onPlaceWindow()
{
    if (m_cfg.displays.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("投放窗口"),
                                 QStringLiteral("请先添加虚拟屏。"));
        return;
    }
    const QVector<MonitorInfo> virtuals = matchedVirtuals();
    m_tabIndex = qBound(0, m_tabIndex, m_cfg.displays.size() - 1);
    if (m_tabIndex >= virtuals.size() || virtuals[m_tabIndex].deviceName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("投放窗口"),
                             QStringLiteral("当前选中的虚拟屏尚未出现在系统显示器列表中。\n"
                                            "请先「添加显示」或加载方案并应用。"));
        return;
    }
    const MonitorInfo mon = virtuals[m_tabIndex];

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("投放窗口到「%1」").arg(m_cfg.displays[m_tabIndex].label));
    dlg.resize(520, 420);
    auto *lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(QStringLiteral("选择要移动的顶层窗口："), &dlg));
    auto *list = new QListWidget(&dlg);
    const QVector<TopWindowInfo> wins = WinDisplay::listTopWindows();
    for (const TopWindowInfo &w : wins) {
        const QString text = w.processName.isEmpty()
                                 ? w.title
                                 : QStringLiteral("%1  —  %2").arg(w.processName, w.title);
        auto *item = new QListWidgetItem(text, list);
        item->setData(Qt::UserRole, QVariant::fromValue(w.hwnd));
    }
    lay->addWidget(list, 1);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
    if (dlg.exec() != QDialog::Accepted || !list->currentItem())
        return;
    const qulonglong hwnd = list->currentItem()->data(Qt::UserRole).toULongLong();
    if (!WinDisplay::moveWindowToMonitor(hwnd, mon.geometry)) {
        QMessageBox::warning(this, QStringLiteral("投放失败"),
                             QStringLiteral("无法移动该窗口（可能已关闭或权限不足）。"));
        return;
    }
    m_title->setStatusHint(QStringLiteral("已投放窗口到「%1」").arg(m_cfg.displays[m_tabIndex].label));
    if (m_previewOn)
        QTimer::singleShot(400, this, &MainWindow::refreshPreview);
}

bool MainWindow::confirmElevate(const QString &action)
{
    return QMessageBox::information(
               this,
               QStringLiteral("需要管理员权限"),
               QStringLiteral("「%1」需要管理员权限。\n\n"
                              "接下来会弹出 UAC。确认后本窗口将关闭，"
                              "并以管理员身份重启后自动完成操作。")
                   .arg(action),
               QMessageBox::Ok | QMessageBox::Cancel,
               QMessageBox::Ok)
        == QMessageBox::Ok;
}

static bool looksSuccess(const QString &title, const QString &msg)
{
    Q_UNUSED(title);
    if (msg.startsWith(QStringLiteral("已应用"))
        || msg.startsWith(QStringLiteral("已更新"))
        || msg.startsWith(QStringLiteral("已添加"))
        || msg.startsWith(QStringLiteral("已删除"))
        || msg.startsWith(QStringLiteral("已就绪"))
        || msg.startsWith(QStringLiteral("已请求禁用"))
        || msg.contains(QStringLiteral("未找到 Parsec"))
        || msg.contains(QStringLiteral("无需清除")))
        return true;
    return !msg.contains(QStringLiteral("失败"));
}

void MainWindow::runBg(const std::function<QString()> &work, const QString &title)
{
    if (m_busy) {
        QMessageBox::information(this, QStringLiteral("请稍候"), QStringLiteral("已有任务在进行中。"));
        return;
    }
    m_busy = true;
    setBusyUi(true);
    m_title->setStatusHint(title + QStringLiteral("中…"));
    m_preview->setGuide(
        title + QStringLiteral("中…"),
        QStringLiteral("请稍候，正在操作虚拟显示驱动。"),
        QString(),
        QString());
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // 必须在主线程操作：保活 QTimer 不能跨线程 start，否则加屏约 1 秒后被驱动摘掉
    QString msg;
    bool ok = true;
    try {
        msg = work();
    } catch (const std::exception &e) {
        ok = false;
        msg = QString::fromLocal8Bit(e.what());
    } catch (...) {
        ok = false;
        msg = QStringLiteral("未知错误");
    }
    if (ok)
        ok = looksSuccess(title, msg);

    m_busy = false;
    setBusyUi(false);
    m_title->setStatusHint(msg.split(QLatin1Char('\n')).value(0));
    if (!ok) {
        QMessageBox::warning(this, title, msg);
        setPreviewEnabled(false);
        rebuildTabs();
        refreshGuide();
        return;
    }

    if (title == QStringLiteral("添加")) {
        m_cfg.displays.push_back(m_pendingSpec);
        m_tabIndex = m_cfg.displays.size() - 1;
    } else if (title == QStringLiteral("删除") && m_pendingIndex >= 0
               && m_pendingIndex < m_cfg.displays.size()) {
        m_cfg.displays.removeAt(m_pendingIndex);
        if (m_tabIndex >= m_cfg.displays.size())
            m_tabIndex = qMax(0, m_cfg.displays.size() - 1);
    } else if (title == QStringLiteral("更新") && m_pendingIndex >= 0
               && m_pendingIndex < m_cfg.displays.size()) {
        m_cfg.displays[m_pendingIndex] = m_pendingSpec;
    } else if (title == QStringLiteral("清除")) {
        m_cfg.displays.clear();
        m_tabIndex = 0;
    }

    persistCfg();
    rebuildTabs();
    updateDriverUi();
    if (msg.contains(QStringLiteral("警告")) || msg.contains(QStringLiteral("提示：")))
        QMessageBox::information(this, title, msg);

    if (title == QStringLiteral("清除") || m_cfg.displays.isEmpty()) {
        setPreviewEnabled(false);
        refreshGuide();
        return;
    }

    setPreviewEnabled(true);
    m_title->setStatusHint(msg.split(QLatin1Char('\n')).value(0)
                           + QStringLiteral(" · 预览已打开"));
    QTimer::singleShot(500, this, &MainWindow::refreshPreview);
    QTimer::singleShot(1500, this, &MainWindow::refreshPreview);
}

void MainWindow::onApply()
{
    const QStringList errs = m_cfg.validate();
    if (!errs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置错误"), errs.join(QLatin1Char('\n')));
        return;
    }
    if (!m_vdd->driverReady()) {
        const auto ans = QMessageBox::question(
            this, QStringLiteral("缺少驱动"),
            m_vdd->installDriverHint() + QStringLiteral("\n\n是否打开驱动下载页？"));
        if (ans == QMessageBox::Yes)
            openDriverPage();
        refreshGuide();
        return;
    }
    persistCfg();
    const AppConfig c = m_cfg;
    runBg([this, c]() { return m_vdd->applyConfig(c); }, QStringLiteral("应用"));
}

void MainWindow::onClear()
{
    if (QMessageBox::question(this, QStringLiteral("清除全部"),
                              QStringLiteral("移除所有虚拟屏？"))
        != QMessageBox::Yes)
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
    QVector<bool> used(pool.size(), false);
    for (int i = 0; i < m_cfg.displays.size(); ++i) {
        const DisplaySpec &spec = m_cfg.displays[i];
        int best = -1;
        for (int j = 0; j < pool.size(); ++j) {
            if (used[j])
                continue;
            if (pool[j].geometry.width() == spec.width && pool[j].geometry.height() == spec.height) {
                best = j;
                break;
            }
        }
        if (best < 0) {
            for (int j = 0; j < pool.size(); ++j) {
                if (!used[j]) {
                    best = j;
                    break;
                }
            }
        }
        if (best >= 0) {
            used[best] = true;
            out[i] = pool[best];
        }
    }
    return out;
}

void MainWindow::refreshPreview()
{
    if (!m_previewOn)
        return;
    if (m_grabBusy || isMinimized() || !isVisible() || m_busy)
        return;

    if (m_cfg.displays.isEmpty()) {
        m_preview->setPlaceholder(QStringLiteral("还没有虚拟屏 · 点「添加显示」"));
        return;
    }
    if (m_tabs.size() != m_cfg.displays.size())
        rebuildTabs();
    m_tabIndex = qBound(0, m_tabIndex, m_cfg.displays.size() - 1);

    const QVector<MonitorInfo> virtuals = matchedVirtuals();
    const DisplaySpec &spec = m_cfg.displays[m_tabIndex];
    const QString label = spec.label;
    if (m_tabIndex >= virtuals.size() || virtuals[m_tabIndex].deviceName.isEmpty()) {
        m_preview->setPlaceholder(
            QStringLiteral("「%1」未出现在系统显示器列表中\n"
                           "请点「添加显示」或加载方案后应用")
                .arg(label));
        m_title->setStatusHint(QStringLiteral("%1 · 系统未挂上").arg(label));
        return;
    }
    const MonitorInfo mon = virtuals[m_tabIndex];
    const QSize target = m_preview->size();
    const int tab = m_tabIndex;

    m_title->setStatusHint(QStringLiteral("%1  %2×%3 @%4Hz  %5")
                               .arg(label)
                               .arg(spec.width)
                               .arg(spec.height)
                               .arg(spec.hz)
                               .arg(mon.deviceName));

    m_grabBusy = true;
    auto *th = QThread::create([this, mon, target, label, tab]() {
        QImage img = WinDisplay::captureDesktopRect(mon.geometry);
        if (!img.isNull() && target.width() > 1 && target.height() > 1
            && (img.width() > target.width() || img.height() > target.height())) {
            img = img.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        QMetaObject::invokeMethod(this, [this, img, label, tab]() {
            m_grabBusy = false;
            if (!m_previewOn || tab != m_tabIndex || m_busy)
                return;
            if (img.isNull())
                m_preview->setPlaceholder(QStringLiteral("抓屏失败"));
            else
                m_preview->setPixmap(QPixmap::fromImage(img));
            Q_UNUSED(label);
        }, Qt::QueuedConnection);
    });
    connect(th, &QThread::finished, th, &QObject::deleteLater);
    th->start();
}
