#include "MainWindow.h"

#include "Elevate.h"
#include "PreviewPane.h"
#include "SettingsPanel.h"
#include "TitleBar.h"
#include "VddService.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
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

    // Tab 行：方案下拉 + 虚拟屏 Tab + 预览开关
    m_tabBar = new QWidget(this);
    m_tabBar->setFixedHeight(32);
    m_tabBar->setStyleSheet(QStringLiteral(
        "background:#0B1220;"
        "QLabel { color:#94A3B8; }"
        "QComboBox {"
        "  padding:2px 8px; border:1px solid #334155; background:#111827; color:#E2E8F0;"
        "  min-width:140px;"
        "}"
        "QComboBox QAbstractItemView { background:#111827; color:#E2E8F0; selection-background-color:#0F766E; }"));
    m_tabLay = new QHBoxLayout(m_tabBar);
    m_tabLay->setContentsMargins(8, 3, 8, 3);
    m_tabLay->setSpacing(6);

    m_profileLabel = new QLabel(QStringLiteral("方案"), m_tabBar);
    m_profileCombo = new QComboBox(m_tabBar);
    m_profileCombo->setToolTip(QStringLiteral("切换已有配置方案；改完后点顶栏「应用」"));
    m_tabLay->addWidget(m_profileLabel);
    m_tabLay->addWidget(m_profileCombo);
    m_tabLay->addSpacing(8);
    m_tabLay->addStretch();

    m_previewToggle = new QPushButton(QStringLiteral("预览:关"), m_tabBar);
    m_previewToggle->setFlat(true);
    m_previewToggle->setCursor(Qt::PointingHandCursor);
    m_previewToggle->setMinimumWidth(72);
    m_previewToggle->setStyleSheet(QStringLiteral(
        "QPushButton { color:#fff; background:#334155; padding:4px 14px; border:none; font-weight:600; }"
        "QPushButton:hover { background:#475569; }"
        "QPushButton:disabled { color:#64748B; }"));
    m_tabLay->addWidget(m_previewToggle);
    root->addWidget(m_tabBar);

    m_preview = new PreviewPane(this);
    root->addWidget(m_preview, 1);

    m_settings = new SettingsDialog(this);
    m_settings->loadFrom(m_cfg);

    connect(m_title, &TitleBar::applyClicked, this, &MainWindow::onApply);
    connect(m_title, &TitleBar::clearClicked, this, &MainWindow::onClear);
    connect(m_title, &TitleBar::settingsClicked, this, &MainWindow::toggleSettings);
    connect(m_title, &TitleBar::closeClicked, this, &QWidget::close);
    connect(m_previewToggle, &QPushButton::clicked, this, &MainWindow::togglePreview);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::onMainProfileChanged);
    connect(m_settings, &SettingsDialog::saveRequested, this, &MainWindow::onSaveSettings);
    connect(m_settings, &SettingsDialog::saveAsRequested, this, &MainWindow::onSaveAsSettings);
    connect(m_settings, &SettingsDialog::loadProfileRequested, this, &MainWindow::onLoadProfile);
    connect(m_settings, &SettingsDialog::browseLoadRequested, this, &MainWindow::onBrowseLoadSettings);
    connect(m_preview, &PreviewPane::primaryClicked, this, &MainWindow::onGuidePrimary);
    connect(m_preview, &PreviewPane::secondaryClicked, this, &MainWindow::onGuideSecondary);
    connect(m_vdd, &VddService::progress, this, [this](const QString &m) {
        m_title->setStatusHint(m);
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshPreview);
    m_timer->start(qMax(1500, m_cfg.previewIntervalMs));

    refreshProfileCombo();
    rebuildTabs();
    if (m_vdd->driverReady())
        m_title->setStatusHint(QStringLiteral("驱动就绪 · 选方案后点「应用」"));
    else
        updateDriverUi();
    refreshGuide();
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

void MainWindow::updateDriverUi()
{
    if (m_vdd->driverReady()) {
        m_settings->setDriverHint(QString());
        return;
    }
    m_title->setStatusHint(QStringLiteral("未检测到驱动"));
    m_settings->setDriverHint(m_vdd->installDriverHint());
}

void MainWindow::refreshProfileCombo()
{
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    m_profileCombo->addItem(QStringLiteral("（当前）%1")
                                .arg(m_cfg.profileName.isEmpty() ? QStringLiteral("未命名")
                                                                 : m_cfg.profileName),
                            QString());
    const QStringList paths = listProfilePaths();
    int sel = 0;
    for (int i = 0; i < paths.size(); ++i) {
        const QString name = QFileInfo(paths[i]).completeBaseName();
        m_profileCombo->addItem(name, paths[i]);
        if (!m_cfg.profileName.isEmpty() && name == m_cfg.profileName)
            sel = i + 1;
    }
    m_profileCombo->setCurrentIndex(sel);
    m_profileCombo->blockSignals(false);
}

void MainWindow::onMainProfileChanged(int index)
{
    const QString path = m_profileCombo->itemData(index).toString();
    if (path.isEmpty())
        return;
    onLoadProfile(path);
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
    m_previewToggle->setStyleSheet(m_previewOn
        ? QStringLiteral(
              "QPushButton { color:#fff; background:#0F766E; padding:4px 14px; border:none; font-weight:600; }"
              "QPushButton:hover { background:#0D9488; }"
              "QPushButton:disabled { color:#64748B; }")
        : QStringLiteral(
              "QPushButton { color:#fff; background:#334155; padding:4px 14px; border:none; font-weight:600; }"
              "QPushButton:hover { background:#475569; }"
              "QPushButton:disabled { color:#64748B; }"));
    if (m_previewOn)
        refreshPreview();
    else
        refreshGuide();
}

void MainWindow::setBusyUi(bool busy)
{
    m_title->setBusy(busy);
    m_profileCombo->setEnabled(!busy);
    m_previewToggle->setEnabled(!busy);
    for (QPushButton *b : m_tabs)
        b->setEnabled(!busy);
}

void MainWindow::openDriverPage()
{
    QDesktopServices::openUrl(QUrl(QLatin1String(kDriverReleasesUrl)));
    m_title->setStatusHint(QStringLiteral("已打开驱动下载页"));
}

void MainWindow::refreshGuide()
{
    if (m_previewOn || m_busy)
        return;

    if (!m_vdd->driverReady()) {
        m_preview->setGuide(
            QStringLiteral("还差一步：安装虚拟显示驱动"),
            QStringLiteral(
                "1. 点下方「打开驱动下载页」，安装 Parsec VDD（ParsecVDisplay）\n"
                "2. 装好后设备里应出现 Parsec Virtual Display Adapter\n"
                "3. 回到本程序，点顶栏「应用」创建虚拟屏\n"
                "（建议先关掉官方 ParsecVDisplay，避免抢控）"),
            QStringLiteral("打开驱动下载页"),
            QStringLiteral("查看安装说明"));
        return;
    }

    const QString profile = m_cfg.profileName.isEmpty() ? QStringLiteral("当前配置") : m_cfg.profileName;
    QString body = QStringLiteral(
                       "1. 上方下拉选择配置方案（当前：%1）\n"
                       "2. 点顶栏「应用」创建虚拟屏（首次会弹 UAC）\n"
                       "3. 点右上角「预览:开」查看画面\n"
                       "\n需要改分辨率/增删屏：点「设置」→「保存方案」")
                       .arg(profile);
    m_preview->setGuide(
        QStringLiteral("三步即可"),
        body,
        QStringLiteral("应用当前配置"),
        QStringLiteral("打开预览"));
}

void MainWindow::onGuidePrimary()
{
    if (!m_vdd->driverReady())
        openDriverPage();
    else
        onApply();
}

void MainWindow::onGuideSecondary()
{
    if (!m_vdd->driverReady()) {
        QMessageBox::information(this, QStringLiteral("安装说明"), m_vdd->installDriverHint());
        return;
    }
    setPreviewEnabled(true);
}

void MainWindow::rebuildTabs()
{
    for (QPushButton *b : m_tabs) {
        m_tabLay->removeWidget(b);
        b->deleteLater();
    }
    m_tabs.clear();

    // 插在 stretch 之前：layout = 方案 | tabs… | stretch | 预览
    const int insertAt = m_tabLay->count() - 2;
    for (int i = 0; i < m_cfg.displays.size(); ++i) {
        const DisplaySpec &spec = m_cfg.displays[i];
        const QString text = spec.label.trimmed().isEmpty()
                                 ? QStringLiteral("虚拟屏%1").arg(i + 1)
                                 : spec.label.trimmed();
        auto *btn = new QPushButton(text, m_tabBar);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setEnabled(!m_busy);
        const bool active = (i == m_tabIndex);
        btn->setStyleSheet(active
            ? QStringLiteral("QPushButton { color:#fff; background:#0F766E; padding:3px 12px; border:none; }")
            : QStringLiteral("QPushButton { color:#94A3B8; background:transparent; padding:3px 12px; border:1px solid #334155; }"));
        btn->setToolTip(QStringLiteral("%1  %2×%3 @%4Hz  缩放%5%")
                            .arg(text)
                            .arg(spec.width)
                            .arg(spec.height)
                            .arg(spec.hz)
                            .arg(spec.scale));
        m_tabLay->insertWidget(insertAt + i, btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { selectTab(i); });
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
}

void MainWindow::togglePreview()
{
    setPreviewEnabled(!m_previewOn);
}

void MainWindow::toggleSettings()
{
    updateDriverUi();
    m_settings->loadFrom(m_cfg);
    m_settings->exec();
    // 设置里可能改了规格但未保存；以磁盘/内存当前 cfg 为准刷新引导
    refreshProfileCombo();
    rebuildTabs();
    if (!m_previewOn)
        refreshGuide();
}

void MainWindow::onSaveSettings()
{
    AppConfig c = m_settings->toConfig(m_cfg);
    const QStringList errs = c.validate();
    if (!errs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置错误"), errs.join(QLatin1Char('\n')));
        return;
    }
    c.profileName = m_cfg.profileName.isEmpty() ? QStringLiteral("当前") : m_cfg.profileName;
    c.save();
    m_cfg = c;
    m_settings->setProfileHint(m_cfg.profileName);
    refreshProfileCombo();
    rebuildTabs();
    m_title->setStatusHint(QStringLiteral("方案已保存 · 点顶栏「应用」生效"));
    if (!m_previewOn)
        refreshGuide();
}

void MainWindow::onSaveAsSettings()
{
    AppConfig c = m_settings->toConfig(m_cfg);
    const QStringList errs = c.validate();
    if (!errs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置错误"), errs.join(QLatin1Char('\n')));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        m_settings,
        QStringLiteral("另存配置"),
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
    c.save();
    m_cfg = c;
    m_settings->loadFrom(m_cfg);
    refreshProfileCombo();
    rebuildTabs();
    m_title->setStatusHint(QStringLiteral("已另存: %1 · 点「应用」生效").arg(c.profileName));
    if (!m_previewOn)
        refreshGuide();
}

void MainWindow::onLoadProfile(const QString &path)
{
    if (path.isEmpty())
        return;
    QString err;
    AppConfig c = AppConfig::loadFromFile(path, &err);
    const QStringList errs = c.validate();
    if (!errs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置无效"), errs.join(QLatin1Char('\n')));
        return;
    }
    c.save();
    m_cfg = c;
    m_settings->loadFrom(m_cfg);
    refreshProfileCombo();
    rebuildTabs();
    m_title->setStatusHint(QStringLiteral("已加载: %1 · 点「应用」生效").arg(m_cfg.profileName));
    if (!m_previewOn)
        refreshGuide();
}

void MainWindow::onBrowseLoadSettings()
{
    const QString path = QFileDialog::getOpenFileName(
        m_settings,
        QStringLiteral("加载配置"),
        profilesDir(),
        QStringLiteral("配置 (*.json)"));
    if (!path.isEmpty())
        onLoadProfile(path);
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
    if (title == QStringLiteral("应用"))
        return msg.startsWith(QStringLiteral("已应用"))
            || msg.startsWith(QStringLiteral("已更新"));
    if (title == QStringLiteral("清除"))
        return msg.startsWith(QStringLiteral("已请求禁用"))
            || msg.contains(QStringLiteral("未找到 Parsec"))
            || msg.contains(QStringLiteral("无需清除"));
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
        QStringLiteral("请稍候，正在操作虚拟显示驱动。\n窗口可能会短暂无响应，属正常现象。"),
        QString(),
        QString());

    auto *th = QThread::create([this, work, title]() {
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
        QMetaObject::invokeMethod(this, [this, msg, ok, title]() {
            m_busy = false;
            setBusyUi(false);
            m_title->setStatusHint(msg.split(QLatin1Char('\n')).value(0));
            rebuildTabs();
            updateDriverUi();
            if (!ok) {
                QMessageBox::warning(this, title, msg);
                setPreviewEnabled(false);
                refreshGuide();
                return;
            }
            if (msg.contains(QStringLiteral("警告")))
                QMessageBox::warning(this, title, msg);

            if (title == QStringLiteral("应用")) {
                // 系统枚举虚拟屏常有延迟，先开预览再短轮询，避免误报「未上线」
                setPreviewEnabled(true);
                m_title->setStatusHint(msg.split(QLatin1Char('\n')).value(0)
                                       + QStringLiteral(" · 预览已打开"));
                QTimer::singleShot(800, this, &MainWindow::refreshPreview);
                QTimer::singleShot(2000, this, &MainWindow::refreshPreview);
            } else {
                setPreviewEnabled(false);
                refreshGuide();
            }
        }, Qt::QueuedConnection);
    });
    connect(th, &QThread::finished, th, &QObject::deleteLater);
    th->start();
}

void MainWindow::onApply()
{
    AppConfig c = m_settings->isVisible() ? m_settings->toConfig(m_cfg) : m_cfg;
    const QStringList errs = c.validate();
    if (!errs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置错误"), errs.join(QLatin1Char('\n')));
        return;
    }
    if (!m_vdd->driverReady()) {
        const auto ans = QMessageBox::question(
            this,
            QStringLiteral("缺少驱动"),
            m_vdd->installDriverHint() + QStringLiteral("\n\n是否打开驱动下载页？"));
        if (ans == QMessageBox::Yes)
            openDriverPage();
        refreshGuide();
        return;
    }
    c.save();
    m_cfg = c;
    rebuildTabs();

    if (!Elevate::isAdmin()) {
        if (!confirmElevate(QStringLiteral("应用")))
            return;
        if (!Elevate::relaunchAsAdmin({QStringLiteral("--apply")}))
            QMessageBox::warning(this, QStringLiteral("提权"), QStringLiteral("无法弹出 UAC，或已取消。"));
        else
            close();
        return;
    }

    runBg([this, c]() { return m_vdd->applyConfig(c); }, QStringLiteral("应用"));
}

void MainWindow::onClear()
{
    if (QMessageBox::question(this, QStringLiteral("清除"),
                              QStringLiteral("禁用虚拟显示驱动？\n清除后虚拟屏应从系统消失，桌面卡顿通常会缓解。"))
        != QMessageBox::Yes)
        return;
    if (!Elevate::isAdmin()) {
        if (!confirmElevate(QStringLiteral("清除")))
            return;
        if (!Elevate::relaunchAsAdmin({QStringLiteral("--clear")}))
            QMessageBox::warning(this, QStringLiteral("提权"), QStringLiteral("无法弹出 UAC，或已取消。"));
        else
            close();
        return;
    }
    runBg([this]() {
        return m_vdd->clearVirtualDisplays();
    }, QStringLiteral("清除"));
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
        m_preview->setPlaceholder(QStringLiteral("当前配置没有虚拟屏"));
        return;
    }
    if (m_tabs.size() != m_cfg.displays.size())
        rebuildTabs();
    m_tabIndex = qBound(0, m_tabIndex, m_cfg.displays.size() - 1);

    const QVector<MonitorInfo> virtuals = matchedVirtuals();
    const DisplaySpec &spec = m_cfg.displays[m_tabIndex];
    const QString label = spec.label;
    if (m_tabIndex >= virtuals.size() || virtuals[m_tabIndex].deviceName.isEmpty()) {
        // 配置可能已写入，但 Windows 桌面尚未挂上该屏（与「没点应用」不是一回事）
        m_preview->setPlaceholder(
            QStringLiteral("「%1」未出现在系统显示器列表中\n"
                           "可再点顶栏「应用」；仍没有则到 Windows「显示设置」看是否多出显示器\n"
                           "（当前机器上活跃显示器里还没有这块虚拟屏）")
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
