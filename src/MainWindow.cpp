#include "MainWindow.h"

#include "Elevate.h"
#include "PreviewPane.h"
#include "SettingsPanel.h"
#include "TitleBar.h"
#include "VddService.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
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
    m_previewToggle = new QPushButton(QStringLiteral("预览:开"), m_tabBar);
    m_previewToggle->setFlat(true);
    m_previewToggle->setCursor(Qt::PointingHandCursor);
    m_previewToggle->setStyleSheet(QStringLiteral(
        "QPushButton { color:#fff; background:#0F766E; padding:2px 10px; border:none; }"
        "QPushButton:hover { background:#0D9488; }"));
    m_tabLay->addWidget(m_previewToggle);
    root->addWidget(m_tabBar);

    m_cap = new QLabel(this);
    m_cap->setStyleSheet(QStringLiteral("color:#94A3B8; padding:2px 10px; background:#0B1220;"));
    m_cap->setFixedHeight(18);
    root->addWidget(m_cap);

    m_preview = new PreviewPane(this);
    root->addWidget(m_preview, 1);

    m_settings = new SettingsPanel(this);
    m_settings->hide();
    m_settings->loadFrom(m_cfg);

    connect(m_title, &TitleBar::applyClicked, this, &MainWindow::onApply);
    connect(m_title, &TitleBar::clearClicked, this, &MainWindow::onClear);
    connect(m_title, &TitleBar::settingsClicked, this, &MainWindow::toggleSettings);
    connect(m_title, &TitleBar::closeClicked, this, &QWidget::close);
    connect(m_previewToggle, &QPushButton::clicked, this, &MainWindow::togglePreview);
    connect(m_settings, &SettingsPanel::applyRequested, this, &MainWindow::onApply);
    connect(m_settings, &SettingsPanel::saveRequested, this, &MainWindow::onSaveSettings);
    connect(m_settings, &SettingsPanel::clearRequested, this, &MainWindow::onClear);
    connect(m_settings, &SettingsPanel::closeRequested, this, &MainWindow::toggleSettings);
    connect(m_vdd, &VddService::progress, this, [this](const QString &m) {
        m_title->setStatusHint(m);
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshPreview);
    m_timer->start(qMax(500, m_cfg.previewIntervalMs));

    rebuildTabs();
    refreshPreview();
}

void MainWindow::rebuildTabs()
{
    for (QPushButton *b : m_tabs) {
        m_tabLay->removeWidget(b);
        b->deleteLater();
    }
    m_tabs.clear();

    const auto targets = WinDisplay::previewTargets(true);
    for (int i = 0; i < targets.size(); ++i) {
        const QString text = targets[i].likelyVirtual
            ? QStringLiteral("虚拟屏 %1").arg(i + 1)
            : targets[i].deviceName.section(QLatin1Char('\\'), -1);
        auto *btn = new QPushButton(text, m_tabBar);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        const bool active = (i == m_tabIndex);
        btn->setStyleSheet(active
            ? QStringLiteral("QPushButton { color:#fff; background:#0F766E; padding:2px 12px; border:none; }")
            : QStringLiteral("QPushButton { color:#94A3B8; background:transparent; padding:2px 12px; border:1px solid #334155; }"));
        m_tabLay->insertWidget(m_tabLay->count() - 2, btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { selectTab(i); });
        m_tabs.push_back(btn);
    }
    if (m_tabIndex >= targets.size())
        m_tabIndex = qMax(0, targets.size() - 1);
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
    if (m_settings->isVisible()) {
        m_settings->hide();
        return;
    }
    m_settings->loadFrom(m_cfg);
    const int w = qMin(420, width() * 2 / 5);
    m_settings->setParent(this);
    m_settings->setGeometry(width() - w, m_title->height(), w, height() - m_title->height());
    m_settings->show();
    m_settings->raise();
}

void MainWindow::onSaveSettings()
{
    AppConfig c = m_settings->toConfig(m_cfg);
    const QStringList errs = c.validate();
    if (!errs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置错误"), errs.join(QLatin1Char('\n')));
        return;
    }
    c.save();
    m_cfg = c;
    m_title->setStatusHint(QStringLiteral("配置已保存"));
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
    if (QMessageBox::question(this, QStringLiteral("清除"), QStringLiteral("禁用虚拟显示驱动设备？"))
        != QMessageBox::Yes)
        return;
    if (!Elevate::isAdmin()) {
        if (!Elevate::relaunchAsAdmin({QStringLiteral("--clear")}))
            QMessageBox::warning(this, QStringLiteral("提权"), QStringLiteral("无法弹出 UAC，或已取消。"));
        else
            close();
        return;
    }
    runBg([this]() { return m_vdd->clearVirtualDisplays(); }, QStringLiteral("清除"));
}

QPixmap MainWindow::grabMonitor(const MonitorInfo &mon) const
{
    // 用虚拟桌面坐标匹配 QScreen，再 grab（Qt 路径，避免 Tk/GDI 抢绘）
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        if (s->geometry() == mon.geometry || s->name().contains(mon.deviceName.section(QLatin1Char('.'), -1))) {
            return s->grabWindow(0);
        }
    }
    for (QScreen *s : screens) {
        if (s->geometry().intersects(mon.geometry.adjusted(8, 8, -8, -8)))
            return s->grabWindow(0);
    }
    if (!screens.isEmpty())
        return screens.first()->grabWindow(0);
    return {};
}

void MainWindow::refreshPreview()
{
    if (!m_previewOn) {
        m_preview->setPlaceholder(QStringLiteral("预览已关闭\n点右上角打开"));
        return;
    }
    const auto targets = WinDisplay::previewTargets(true);
    if (targets.isEmpty()) {
        m_cap->setText(QStringLiteral("没有可预览的监视器"));
        m_preview->setPlaceholder(QStringLiteral("没有可预览的监视器\n请先「应用」虚拟屏"));
        return;
    }
    if (m_tabs.size() != targets.size())
        rebuildTabs();
    m_tabIndex = qBound(0, m_tabIndex, targets.size() - 1);
    const MonitorInfo &mon = targets[m_tabIndex];
    m_cap->setText(QStringLiteral("%1  %2×%3%4")
                       .arg(mon.deviceName)
                       .arg(mon.geometry.width())
                       .arg(mon.geometry.height())
                       .arg(mon.likelyVirtual ? QStringLiteral("  · 虚拟") : QString()));
    const QPixmap pm = grabMonitor(mon);
    if (pm.isNull())
        m_preview->setPlaceholder(QStringLiteral("抓屏失败"));
    else
        m_preview->setPixmap(pm);
}
