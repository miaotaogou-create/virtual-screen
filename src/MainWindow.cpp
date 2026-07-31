#include "MainWindow.h"

#include "Elevate.h"
#include "PreviewPane.h"
#include "SettingsPanel.h"
#include "TitleBar.h"
#include "VddService.h"

#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

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
    m_title->setStatusHint(m_vdd->driverReady() ? QStringLiteral("驱动就绪") : QStringLiteral("未检测到驱动"));
    root->addWidget(m_title);

    // 薄 Tab 行
    m_tabBar = new QWidget(this);
    m_tabBar->setFixedHeight(26);
    m_tabBar->setStyleSheet(QStringLiteral("background:#0B1220;"));
    m_tabLay = new QHBoxLayout(m_tabBar);
    m_tabLay->setContentsMargins(8, 2, 8, 2);
    m_tabLay->setSpacing(4);
    m_tabLay->addStretch();
    m_previewToggle = new QPushButton(QStringLiteral("预览:关"), m_tabBar);
    m_previewToggle->setFlat(true);
    m_previewToggle->setCursor(Qt::PointingHandCursor);
    m_previewToggle->setStyleSheet(QStringLiteral(
        "QPushButton { color:#fff; background:#334155; padding:2px 10px; border:none; }"
        "QPushButton:hover { background:#475569; }"));
    m_tabLay->addWidget(m_previewToggle);
    root->addWidget(m_tabBar);

    m_preview = new PreviewPane(this);
    root->addWidget(m_preview, 1);
    m_preview->setPlaceholder(QStringLiteral("预览默认关闭（更流畅）\n需要看画面时点右上角「预览:关」打开"));

    m_settings = new SettingsDialog(this);
    m_settings->loadFrom(m_cfg);

    connect(m_title, &TitleBar::applyClicked, this, &MainWindow::onApply);
    connect(m_title, &TitleBar::clearClicked, this, &MainWindow::onClear);
    connect(m_title, &TitleBar::settingsClicked, this, &MainWindow::toggleSettings);
    connect(m_title, &TitleBar::closeClicked, this, &QWidget::close);
    connect(m_previewToggle, &QPushButton::clicked, this, &MainWindow::togglePreview);
    connect(m_settings, &SettingsDialog::applyRequested, this, &MainWindow::onApply);
    connect(m_settings, &SettingsDialog::saveRequested, this, &MainWindow::onSaveSettings);
    connect(m_settings, &SettingsDialog::saveAsRequested, this, &MainWindow::onSaveAsSettings);
    connect(m_settings, &SettingsDialog::loadProfileRequested, this, &MainWindow::onLoadProfile);
    connect(m_settings, &SettingsDialog::browseLoadRequested, this, &MainWindow::onBrowseLoadSettings);
    connect(m_settings, &SettingsDialog::clearRequested, this, &MainWindow::onClear);
    connect(m_vdd, &VddService::progress, this, [this](const QString &m) {
        m_title->setStatusHint(m);
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshPreview);
    m_timer->start(qMax(1500, m_cfg.previewIntervalMs));

    rebuildTabs();
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

void MainWindow::rebuildTabs()
{
    for (QPushButton *b : m_tabs) {
        m_tabLay->removeWidget(b);
        b->deleteLater();
    }
    m_tabs.clear();

    // Tab 跟当前配置走，不跟系统里碰巧在线的监视器数量绑死
    for (int i = 0; i < m_cfg.displays.size(); ++i) {
        const DisplaySpec &spec = m_cfg.displays[i];
        const QString text = spec.label.trimmed().isEmpty()
                                 ? QStringLiteral("虚拟屏%1").arg(i + 1)
                                 : spec.label.trimmed();
        auto *btn = new QPushButton(text, m_tabBar);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        const bool active = (i == m_tabIndex);
        btn->setStyleSheet(active
            ? QStringLiteral("QPushButton { color:#fff; background:#0F766E; padding:2px 12px; border:none; }")
            : QStringLiteral("QPushButton { color:#94A3B8; background:transparent; padding:2px 12px; border:1px solid #334155; }"));
        btn->setToolTip(QStringLiteral("%1  %2×%3 @%4Hz  缩放%5%")
                            .arg(text)
                            .arg(spec.width)
                            .arg(spec.height)
                            .arg(spec.hz)
                            .arg(spec.scale));
        m_tabLay->insertWidget(m_tabLay->count() - 2, btn);
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
    refreshPreview();
}

void MainWindow::togglePreview()
{
    m_previewOn = !m_previewOn;
    m_previewToggle->setText(m_previewOn ? QStringLiteral("预览:开") : QStringLiteral("预览:关"));
    m_previewToggle->setStyleSheet(m_previewOn
        ? QStringLiteral("QPushButton { color:#fff; background:#0F766E; padding:2px 10px; border:none; }")
        : QStringLiteral("QPushButton { color:#fff; background:#334155; padding:2px 10px; border:none; }"));
    if (m_previewOn)
        refreshPreview();
    else
        m_preview->setPlaceholder(QStringLiteral("预览已关闭（更省资源）\n点右上角「预览:关」打开"));
}

void MainWindow::toggleSettings()
{
    m_settings->loadFrom(m_cfg);
    m_settings->exec();
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
    rebuildTabs();
    m_title->setStatusHint(QStringLiteral("已保存到 config.json"));
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
    c.save(); // 同步为启动默认
    m_cfg = c;
    m_settings->loadFrom(m_cfg);
    rebuildTabs();
    m_title->setStatusHint(QStringLiteral("已另存: %1").arg(c.profileName));
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
    rebuildTabs();
    m_title->setStatusHint(QStringLiteral("已加载: %1").arg(m_cfg.profileName));
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

void MainWindow::runBg(const std::function<QString()> &work, const QString &title)
{
    if (m_busy) {
        QMessageBox::information(this, QStringLiteral("请稍候"), QStringLiteral("已有任务在进行中。"));
        return;
    }
    m_busy = true;
    m_title->setStatusHint(title + QStringLiteral("…"));

    // ponytail: 简单丢到线程；UI 用 QueuedConnection 回传
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
        QMetaObject::invokeMethod(this, [this, msg, ok, title]() {
            m_busy = false;
            m_title->setStatusHint(msg);
            rebuildTabs();
            if (ok)
                QMessageBox::information(this, title, msg);
            else
                QMessageBox::critical(this, title, msg);
            refreshPreview();
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
    c.save();
    m_cfg = c;
    rebuildTabs();

    if (!Elevate::isAdmin()) {
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
        if (!Elevate::relaunchAsAdmin({QStringLiteral("--clear")}))
            QMessageBox::warning(this, QStringLiteral("提权"), QStringLiteral("无法弹出 UAC，或已取消。"));
        else
            close();
        return;
    }
    runBg([this]() {
        const QString msg = m_vdd->clearVirtualDisplays();
        return msg;
    }, QStringLiteral("清除"));
}

QPixmap MainWindow::grabMonitor(const MonitorInfo &mon) const
{
    QPixmap pm;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        if (s->geometry() == mon.geometry || s->name().contains(mon.deviceName.section(QLatin1Char('.'), -1))) {
            pm = s->grabWindow(0);
            break;
        }
    }
    if (pm.isNull()) {
        for (QScreen *s : screens) {
            if (s->geometry().intersects(mon.geometry.adjusted(8, 8, -8, -8))) {
                pm = s->grabWindow(0);
                break;
            }
        }
    }
    if (pm.isNull() && !screens.isEmpty())
        pm = screens.first()->grabWindow(0);
    if (pm.isNull())
        return {};

    // 抓完立刻缩到预览区大小，避免整屏位图常驻 + paint 再平滑缩放
    const QSize target = m_preview ? m_preview->size() : QSize(960, 600);
    if (target.width() > 1 && target.height() > 1
        && (pm.width() > target.width() || pm.height() > target.height())) {
        pm = pm.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return pm;
}

void MainWindow::refreshPreview()
{
    if (!m_previewOn)
        return;
    if (m_grabBusy || isMinimized() || !isVisible())
        return;

    if (m_cfg.displays.isEmpty()) {
        m_preview->setPlaceholder(QStringLiteral("当前配置没有虚拟屏"));
        return;
    }
    if (m_tabs.size() != m_cfg.displays.size())
        rebuildTabs();
    m_tabIndex = qBound(0, m_tabIndex, m_cfg.displays.size() - 1);

    QVector<MonitorInfo> virtuals;
    for (const MonitorInfo &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            virtuals.push_back(m);
    }
    const QString label = m_cfg.displays[m_tabIndex].label;
    if (m_tabIndex >= virtuals.size()) {
        m_preview->setPlaceholder(QStringLiteral("「%1」尚未上线\n请先点「应用」创建虚拟屏").arg(label));
        m_title->setStatusHint(QStringLiteral("%1 · 未上线").arg(label));
        return;
    }
    const MonitorInfo &mon = virtuals[m_tabIndex];

    m_title->setStatusHint(QStringLiteral("%1  %2  %3×%4")
                               .arg(label)
                               .arg(mon.deviceName)
                               .arg(mon.geometry.width())
                               .arg(mon.geometry.height()));

    m_grabBusy = true;
    const QPixmap pm = grabMonitor(mon);
    m_grabBusy = false;
    if (pm.isNull())
        m_preview->setPlaceholder(QStringLiteral("抓屏失败"));
    else
        m_preview->setPixmap(pm);
}
