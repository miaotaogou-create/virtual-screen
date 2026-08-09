#include "VddService.h"

#include "WinDisplay.h"
#include "parsec-vdd.h"

#include <QSettings>
#include <QSet>
#include <QThread>
#include <QTimer>

using namespace parsec_vdd;

namespace {

int countLikelyVirtuals()
{
    int n = 0;
    for (const MonitorInfo &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            ++n;
    }
    return n;
}

QVector<MonitorInfo> waitVirtuals(int want, int rounds = 40)
{
    QVector<MonitorInfo> virtuals;
    for (int i = 0; i < rounds; ++i) {
        virtuals.clear();
        for (const MonitorInfo &m : WinDisplay::listMonitors()) {
            if (m.likelyVirtual)
                virtuals.push_back(m);
        }
        if (virtuals.size() >= want)
            break;
        QThread::msleep(200);
    }
    if (virtuals.size() > want)
        virtuals = virtuals.mid(0, want);
    return virtuals;
}

} // namespace

VddService::VddService(QObject *parent)
    : QObject(parent)
{
    m_ping = new QTimer(this);
    m_ping->setInterval(80); // Parsec：约 100ms 内 ping，否则约 1 秒掉屏
    connect(m_ping, &QTimer::timeout, this, [this]() {
        if (m_parsec)
            VddUpdate(static_cast<HANDLE>(m_parsec));
    });
}

VddService::~VddService()
{
    if (m_parsec && !m_parsecIndices.isEmpty()) {
        for (int i = m_parsecIndices.size() - 1; i >= 0; --i)
            VddRemoveDisplay(static_cast<HANDLE>(m_parsec), m_parsecIndices[i]);
        m_parsecIndices.clear();
    }
    closeParsec();
}

bool VddService::parsecReady() const
{
    return QueryDeviceStatus(&VDD_CLASS_GUID, VDD_HARDWARE_ID) == DEVICE_OK;
}

bool VddService::driverReady() const
{
    return parsecReady();
}

QString VddService::installDriverHint() const
{
    return QStringLiteral(
        "未检测到 Parsec Virtual Display Driver。\n"
        "优先：本程序目录下 parsec-vdd\\parsec-vdd-0.45.0.0.exe（点「安装捆绑驱动」）\n"
        "或仓库：scripts\\install_parsec_vdd.ps1 / vendor\\parsec-vdd\\\n"
        "网页：https://github.com/nomi-san/parsec-vdd/releases\n"
        "装好后设备管理器应出现「Parsec Virtual Display Adapter」。\n"
        "使用本程序时建议先关掉官方 ParsecVDisplay，避免抢控。");
}

bool VddService::ensureParsecOpen(QString *err)
{
    if (m_parsec)
        return true;
    if (!parsecReady()) {
        if (err)
            *err = installDriverHint();
        return false;
    }
    HANDLE h = OpenDeviceHandle(&VDD_ADAPTER_GUID);
    if (h == nullptr || h == INVALID_HANDLE_VALUE) {
        if (err)
            *err = QStringLiteral("无法打开 Parsec VDD 设备句柄（可先关掉 ParsecVDisplay 再试）。");
        return false;
    }
    m_parsec = h;
    startPing();
    return true;
}

void VddService::startPing()
{
    if (m_ping && !m_ping->isActive())
        m_ping->start();
}

void VddService::stopPing()
{
    if (m_ping)
        m_ping->stop();
}

void VddService::closeParsec()
{
    stopPing();
    if (m_parsec) {
        CloseDeviceHandle(static_cast<HANDLE>(m_parsec));
        m_parsec = nullptr;
    }
}

bool VddService::writeParsecCustomModes(const AppConfig &cfg, QString *err)
{
    struct Mode {
        int w, h, hz;
    };
    QVector<Mode> modes;
    QSet<QString> seen;
    for (const DisplaySpec &d : cfg.displays) {
        const QString key = QStringLiteral("%1x%2@%3").arg(d.width).arg(d.height).arg(d.hz);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        modes.push_back({d.width, d.height, d.hz});
        if (modes.size() >= 5)
            break;
    }
    if (modes.isEmpty()) {
        if (err)
            *err = QStringLiteral("没有可写入的分辨率");
        return false;
    }

    {
        QSettings root(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Parsec"), QSettings::NativeFormat);
        root.remove(QStringLiteral("vdd"));
    }
    for (int i = 0; i < modes.size(); ++i) {
        QSettings s(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Parsec\\vdd\\%1").arg(i),
                    QSettings::NativeFormat);
        s.setValue(QStringLiteral("width"), modes[i].w);
        s.setValue(QStringLiteral("height"), modes[i].h);
        s.setValue(QStringLiteral("hz"), modes[i].hz);
        s.sync();
        if (s.status() != QSettings::NoError) {
            if (err)
                *err = QStringLiteral("写入自定义分辨率失败（需要管理员权限）: HKLM\\SOFTWARE\\Parsec\\vdd");
            return false;
        }
    }
    return true;
}

int VddService::countDesktopVirtuals() const
{
    return countLikelyVirtuals();
}

QString VddService::applyConfig(const AppConfig &cfg, QString *detail)
{
    Q_UNUSED(detail);
    QString err;
    if (!ensureParsecOpen(&err))
        return err;

    emit progress(QStringLiteral("写入 Parsec 自定义分辨率…"));
    if (!writeParsecCustomModes(cfg, &err))
        return err;

    const int want = cfg.displays.size();
    if (want > VDD_MAX_DISPLAYS)
        return QStringLiteral("Parsec VDD 建议最多 %1 块虚拟屏。").arg(VDD_MAX_DISPLAYS);

    emit progress(QStringLiteral("调整虚拟屏数量…"));
    for (int i = m_parsecIndices.size() - 1; i >= 0; --i) {
        VddRemoveDisplay(static_cast<HANDLE>(m_parsec), m_parsecIndices[i]);
        QThread::msleep(150);
    }
    m_parsecIndices.clear();
    QThread::msleep(400);

    int onDesktop = countDesktopVirtuals();
    for (int idx = onDesktop - 1; idx >= 0 && onDesktop > 0; --idx) {
        VddRemoveDisplay(static_cast<HANDLE>(m_parsec), idx);
        QThread::msleep(150);
        onDesktop = countDesktopVirtuals();
    }
    QThread::msleep(300);

    for (int i = 0; i < want; ++i) {
        const int idx = VddAddDisplay(static_cast<HANDLE>(m_parsec));
        if (idx < 0)
            return QStringLiteral("添加第 %1 块虚拟屏失败（Parsec ioctl）。").arg(i + 1);
        m_parsecIndices.push_back(idx);
        emit progress(QStringLiteral("已添加虚拟屏 index=%1").arg(idx));
        QThread::msleep(200);
    }

    emit progress(QStringLiteral("等待虚拟屏挂到桌面…"));
    QVector<MonitorInfo> virtuals = waitVirtuals(want);
    if (virtuals.size() < want) {
        return QStringLiteral("已请求 %1 块虚拟屏，但桌面只看到 %2 块。\n"
                              "可到「显示设置」确认是否为扩展模式（不要复制）。")
            .arg(want)
            .arg(virtuals.size());
    }

    emit progress(QStringLiteral("设置分辨率与排列…"));
    MonitorInfo primary;
    bool hasPrimary = false;
    for (const MonitorInfo &m : WinDisplay::listMonitors()) {
        if (m.primary) {
            primary = m;
            hasPrimary = true;
            break;
        }
    }
    if (!hasPrimary)
        return QStringLiteral("找不到主显示器");

    int x = primary.geometry.right();
    int y = primary.geometry.top();
    for (int i = 0; i < want; ++i) {
        const DisplaySpec &spec = cfg.displays[i];
        const MonitorInfo &mon = virtuals[i];
        if (!WinDisplay::setMode(mon.deviceName, spec.width, spec.height, spec.hz, x, y))
            return QStringLiteral("设置分辨率失败: %1（确认自定义分辨率已写入注册表）")
                .arg(mon.deviceName);
        x += spec.width;
    }
    WinDisplay::applyDisplayChanges();

    emit progress(QStringLiteral("设置显示缩放…"));
    QThread::msleep(400);
    int dpiOk = 0;
    int dpiFail = 0;
    QVector<MonitorInfo> after;
    for (const MonitorInfo &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            after.push_back(m);
    }
    for (int i = 0; i < want; ++i) {
        const DisplaySpec &spec = cfg.displays[i];
        QString device = virtuals[i].deviceName;
        for (const MonitorInfo &m : after) {
            if (m.geometry.width() == spec.width && m.geometry.height() == spec.height
                && m.geometry.x() >= primary.geometry.right() - 8) {
                device = m.deviceName;
                break;
            }
        }
        if (WinDisplay::setDpiScale(device, spec.scale))
            ++dpiOk;
        else
            ++dpiFail;
    }

    QString msg = QStringLiteral("已应用 %1 块虚拟屏（Parsec VDD）").arg(want);
    if (dpiOk > 0)
        msg += QStringLiteral("，缩放成功 %1").arg(dpiOk);
    if (dpiFail > 0)
        msg += QStringLiteral("，缩放失败 %1（可到系统显示设置手动调）").arg(dpiFail);
    msg += QStringLiteral("。请保持本程序运行以维持虚拟屏（关闭后约 1 秒会自动卸屏）。");
    return msg;
}

QString VddService::clearVirtualDisplays()
{
    QString err;
    if (!ensureParsecOpen(&err))
        return QStringLiteral("未找到 Parsec VDD，无需清除。");

    emit progress(QStringLiteral("移除 Parsec 虚拟屏…"));
    for (int i = m_parsecIndices.size() - 1; i >= 0; --i) {
        VddRemoveDisplay(static_cast<HANDLE>(m_parsec), m_parsecIndices[i]);
        QThread::msleep(120);
    }
    m_parsecIndices.clear();

    int left = countDesktopVirtuals();
    for (int idx = left - 1; idx >= 0; --idx) {
        VddRemoveDisplay(static_cast<HANDLE>(m_parsec), idx);
        QThread::msleep(120);
    }

    QThread::msleep(500);
    left = countDesktopVirtuals();
    closeParsec();

    if (left > 0)
        return QStringLiteral("已请求清除，但桌面仍看到 %1 块虚拟屏。可再试一次或重启。").arg(left);
    return QStringLiteral("已请求禁用虚拟屏，虚拟屏已从桌面移除。");
}
