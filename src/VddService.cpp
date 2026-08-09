#include "VddService.h"

#include "WinDisplay.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QTextStream>
#include <algorithm>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef CREATE_NO_WINDOW
#define CREATE_NO_WINDOW 0x08000000
#endif

namespace {

QString preferGpuFriendlyName()
{
    // 独显优先；笔记本上 VDD 绑核显时 swapchain 更容易挂
    QString nvidia;
    QString any;
    for (DWORD i = 0;; ++i) {
        DISPLAY_DEVICEW adapter{};
        adapter.cb = sizeof(adapter);
        if (!EnumDisplayDevicesW(nullptr, i, &adapter, 0))
            break;
        const QString name = QString::fromWCharArray(adapter.DeviceString);
        if (name.isEmpty())
            continue;
        if (name.contains(QStringLiteral("Microsoft Basic Render"), Qt::CaseInsensitive))
            continue;
        if (name.contains(QStringLiteral("Virtual Display"), Qt::CaseInsensitive))
            continue;
        if (any.isEmpty())
            any = name;
        if (name.contains(QStringLiteral("NVIDIA"), Qt::CaseInsensitive)
            || name.contains(QStringLiteral("GeForce"), Qt::CaseInsensitive)
            || name.contains(QStringLiteral("Radeon"), Qt::CaseInsensitive)) {
            nvidia = name;
            break;
        }
    }
    if (!nvidia.isEmpty())
        return nvidia;
    if (!any.isEmpty())
        return any;
    return QStringLiteral("default");
}

QByteArray makeVddXml(const QVector<DisplaySpec> &displays, const QString &gpuName)
{
    QString xml;
    QTextStream ts(&xml);
    ts << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    ts << "<vdd_settings>\n  <monitors>\n    <count>" << displays.size() << "</count>\n  </monitors>\n";
    ts << "  <gpu>\n    <friendlyname>" << gpuName << "</friendlyname>\n  </gpu>\n";
    ts << "  <global>\n";
    QList<int> hzList;
    for (const DisplaySpec &d : displays) {
        if (!hzList.contains(d.hz))
            hzList.append(d.hz);
    }
    std::sort(hzList.begin(), hzList.end());
    for (int hz : hzList)
        ts << "    <g_refresh_rate>" << hz << "</g_refresh_rate>\n";
    ts << "  </global>\n  <resolutions>\n";
    QSet<QString> seen;
    for (const DisplaySpec &d : displays) {
        const QString key = QStringLiteral("%1x%2@%3").arg(d.width).arg(d.height).arg(d.hz);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        ts << "    <resolution>\n"
           << "      <width>" << d.width << "</width>\n"
           << "      <height>" << d.height << "</height>\n"
           << "      <refresh_rate>" << d.hz << "</refresh_rate>\n"
           << "    </resolution>\n";
    }
    // HardwareCursor 在部分独显笔记本上会触发 swapchain 失败，先关
    ts << "  </resolutions>\n"
       << "  <options>\n"
       << "    <CustomEdid>false</CustomEdid>\n"
       << "    <PreventSpoof>true</PreventSpoof>\n"
       << "    <HardwareCursor>false</HardwareCursor>\n"
       << "    <logging>true</logging>\n"
       << "  </options>\n"
       << "</vdd_settings>\n";
    return xml.toUtf8();
}

QString runHidden(const QString &program, const QStringList &args, int *exitCode = nullptr)
{
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *cpa) {
        cpa->flags |= CREATE_NO_WINDOW;
    });
    p.start();
    if (!p.waitForFinished(120000)) {
        if (exitCode)
            *exitCode = -1;
        return QStringLiteral("超时");
    }
    if (exitCode)
        *exitCode = p.exitCode();
    return QString::fromLocal8Bit(p.readAll());
}

QStringList findVddInstanceIds()
{
    QStringList ids;

    {
        int code = 0;
        const QString out = runHidden(QStringLiteral("pnputil"),
                                      {QStringLiteral("/enum-devices"), QStringLiteral("/class"), QStringLiteral("Display")},
                                      &code);
        QString curId;
        QString curDesc;
        const QStringList lines = out.split(QRegularExpression(QStringLiteral("[\r\n]")), QString::SkipEmptyParts);
        auto isIdLine = [](const QString &line) {
            const QString low = line.toLower();
            return low.contains(QStringLiteral("instance id"))
                || line.contains(QStringLiteral("实例 ID"))
                || line.contains(QStringLiteral("实例ID"))
                || line.contains(QStringLiteral("实例 id"));
        };
        auto isDescLine = [](const QString &line) {
            const QString low = line.toLower();
            return low.contains(QStringLiteral("device description"))
                || line.contains(QStringLiteral("设备描述"));
        };
        auto flush = [&]() {
            const QString blob = (curDesc + QLatin1Char(' ') + curId).toLower();
            const bool byName = blob.contains(QStringLiteral("virtual display"))
                || blob.contains(QStringLiteral("mttvdd"))
                || blob.contains(QStringLiteral("iddsample"))
                || blob.contains(QStringLiteral("indirect display"))
                || blob.contains(QStringLiteral("vdd by"))
                || blob.contains(QStringLiteral("mikethetech"));
            if (byName && !curId.isEmpty())
                ids << curId;
            curId.clear();
            curDesc.clear();
        };
        for (QString line : lines) {
            line = line.trimmed();
            if (line.isEmpty())
                continue;
            if (isIdLine(line)) {
                flush();
                int colon = line.indexOf(QLatin1Char(':'));
                if (colon < 0)
                    colon = line.indexOf(QChar(0xFF1A));
                curId = colon >= 0 ? line.mid(colon + 1).trimmed() : QString();
            } else if (isDescLine(line)) {
                int colon = line.indexOf(QLatin1Char(':'));
                if (colon < 0)
                    colon = line.indexOf(QChar(0xFF1A));
                curDesc = colon >= 0 ? line.mid(colon + 1).trimmed() : QString();
            }
        }
        flush();
    }

    if (ids.isEmpty()) {
        int code = 0;
        const QString out = runHidden(
            QStringLiteral("powershell"),
            {QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
             QStringLiteral("Get-PnpDevice -Class Display -ErrorAction SilentlyContinue |"
                            " Where-Object { $_.FriendlyName -match 'Virtual Display|MttVDD|IddSample|Indirect Display|VDD by' } |"
                            " ForEach-Object { $_.InstanceId }")},
            &code);
        for (QString line : out.split(QRegularExpression(QStringLiteral("[\r\n]")), QString::SkipEmptyParts)) {
            line = line.trimmed();
            if (line.contains(QLatin1Char('\\')))
                ids << line;
        }
    }

    ids.removeDuplicates();
    return ids;
}

bool pnpSetEnabled(const QString &instanceId, bool enabled)
{
    int code = 0;
    runHidden(QStringLiteral("pnputil"),
              {enabled ? QStringLiteral("/enable-device") : QStringLiteral("/disable-device"), instanceId},
              &code);
    if (code == 0)
        return true;

    const QString cmd = enabled
        ? QStringLiteral("Enable-PnpDevice -InstanceId '%1' -Confirm:$false").arg(instanceId)
        : QStringLiteral("Disable-PnpDevice -InstanceId '%1' -Confirm:$false").arg(instanceId);
    int psCode = 0;
    runHidden(QStringLiteral("powershell"),
              {QStringLiteral("-NoProfile"), QStringLiteral("-Command"), cmd},
              &psCode);
    return psCode == 0;
}

bool pnpRestart(const QString &instanceId)
{
    int code = 0;
    runHidden(QStringLiteral("pnputil"),
              {QStringLiteral("/restart-device"), instanceId},
              &code);
    if (code == 0)
        return true;
    // 旧系统无 /restart-device：禁用再启用
    if (!pnpSetEnabled(instanceId, false))
        return false;
    QThread::msleep(400);
    return pnpSetEnabled(instanceId, true);
}

/** 官方控制通路：\\\\.\\pipe\\MTTVirtualDisplayPipe，命令 UTF-16LE。 */
QString pipeCommand(const QString &cmd, int waitMs = 2500)
{
    const wchar_t *pipeName = L"\\\\.\\pipe\\MTTVirtualDisplayPipe";
    if (!WaitNamedPipeW(pipeName, 3000)) {
        const DWORD e = GetLastError();
        if (e != ERROR_SEM_TIMEOUT && e != ERROR_FILE_NOT_FOUND)
            return QStringLiteral("PIPE_WAIT_FAIL %1").arg(e);
    }
    HANDLE h = CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return QStringLiteral("PIPE_OPEN_FAIL %1").arg(GetLastError());

    const std::wstring w = cmd.toStdWString();
    DWORD written = 0;
    const BOOL okWrite = WriteFile(h, w.data(), DWORD(w.size() * sizeof(wchar_t)), &written, nullptr);
    if (!okWrite) {
        const DWORD e = GetLastError();
        CloseHandle(h);
        return QStringLiteral("PIPE_WRITE_FAIL %1").arg(e);
    }

    QByteArray out;
    const DWORD start = GetTickCount();
    char buf[2048];
    while (int(GetTickCount() - start) < waitMs) {
        DWORD avail = 0;
        if (!PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr))
            break;
        if (avail == 0) {
            if (!out.isEmpty() && int(GetTickCount() - start) > 600)
                break;
            Sleep(40);
            continue;
        }
        DWORD n = 0;
        if (!ReadFile(h, buf, qMin<DWORD>(avail, sizeof(buf)), &n, nullptr) || n == 0)
            break;
        out.append(buf, int(n));
    }
    CloseHandle(h);
    return QString::fromUtf8(out);
}

bool pipeAlive()
{
    const wchar_t *pipeName = L"\\\\.\\pipe\\MTTVirtualDisplayPipe";
    // 管道存在即可；PING 有的版本无正文，不能靠回包判断
    if (WaitNamedPipeW(pipeName, 500))
        return true;
    const DWORD e = GetLastError();
    return e == ERROR_PIPE_BUSY || e == ERROR_SEM_TIMEOUT;
}

int countDesktopVirtuals()
{
    int n = 0;
    for (const MonitorInfo &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            ++n;
    }
    return n;
}

int countPhantomVirtualAdapters()
{
    // EnumDisplayMonitors 看不到、但适配器列表里有的 VDD 目标
    int n = 0;
    for (DWORD i = 0;; ++i) {
        DISPLAY_DEVICEW adapter{};
        adapter.cb = sizeof(adapter);
        if (!EnumDisplayDevicesW(nullptr, i, &adapter, 0))
            break;
        const QString name = QString::fromWCharArray(adapter.DeviceString);
        if (!name.contains(QStringLiteral("Virtual Display"), Qt::CaseInsensitive)
            && !name.contains(QStringLiteral("MttVDD"), Qt::CaseInsensitive))
            continue;
        if (!(adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
            ++n;
    }
    return n;
}

QVector<MonitorInfo> waitDesktopVirtuals(int want, int rounds = 40)
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

QString diagnoseMissingVirtuals(int got, int want)
{
    const int phantoms = countPhantomVirtualAdapters();
    QString msg = QStringLiteral("已写入配置，但桌面只挂上 %1 块虚拟屏（期望 %2）。").arg(got).arg(want);
    if (phantoms > 0) {
        msg += QStringLiteral(
            "\n驱动侧还能看到 %1 个未挂桌面的 Virtual Display 适配器（常见于 swapchain 失败）。"
            "\n请：①关掉 Virtual Driver Control（官方配套）避免抢配置；"
            "②设备管理器里对「Virtual Display Driver」禁用再启用，或重启电脑；"
            "③查看 C:\\VirtualDisplayDriver\\Logs 是否有 Failed to set device to swap chain。")
                   .arg(phantoms);
    } else {
        msg += QStringLiteral("\n若刚改过 GPU/屏数，请再点一次「应用」；仍不行请重启 Virtual Display Driver 设备。");
    }
    return msg;
}

} // namespace

VddService::VddService(QObject *parent)
    : QObject(parent)
{
}

bool VddService::driverReady() const
{
    if (QDir(QStringLiteral("C:/VirtualDisplayDriver")).exists())
        return true;
    for (const auto &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            return true;
    }
    return !findVddInstanceIds().isEmpty();
}

QString VddService::installDriverHint() const
{
    return QStringLiteral(
        "未检测到 Virtual Display Driver。请先安装驱动：\n"
        "1) 官方：https://github.com/VirtualDrivers/Virtual-Display-Driver/releases\n"
        "2) 或管理员运行仓库 scripts\\install_vdd.ps1\n"
        "装好后通常出现 C:\\VirtualDisplayDriver，再点「应用」。\n"
        "请先关掉官方 Virtual Driver Control，避免与本程序抢配置。");
}

QString VddService::clearVirtualDisplays()
{
    {
        AppConfig empty = AppConfig::defaults();
        empty.displays.clear();
        QFile f(empty.vddSettingsPath);
        QDir().mkpath(QFileInfo(empty.vddSettingsPath).absolutePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<vdd_settings>\n  <monitors>\n    <count>0</count>\n  </monitors>\n"
                    "  <gpu>\n    <friendlyname>default</friendlyname>\n  </gpu>\n"
                    "  <global/>\n  <resolutions/>\n  <options>\n"
                    "    <CustomEdid>false</CustomEdid>\n"
                    "    <PreventSpoof>true</PreventSpoof>\n"
                    "    <HardwareCursor>false</HardwareCursor>\n"
                    "    <logging>false</logging>\n"
                    "  </options>\n</vdd_settings>\n");
        }
    }

    // 优先走官方管道把屏数打到 0（会内部 reload）
    if (pipeAlive()) {
        emit progress(QStringLiteral("经驱动管道清除虚拟屏…"));
        pipeCommand(QStringLiteral("SETDISPLAYCOUNT 0"), 2000);
        QThread::msleep(1200);
    }

    const QStringList ids = findVddInstanceIds();
    if (ids.isEmpty()) {
        int left = countDesktopVirtuals();
        if (left > 0)
            return QStringLiteral("仍检测到 %1 块虚拟屏，但找不到驱动设备实例（清除失败）。").arg(left);
        return QStringLiteral("未找到虚拟显示驱动设备（可能已清除）。");
    }

    emit progress(QStringLiteral("正在禁用虚拟显示驱动…"));
    int n = 0;
    QStringList failed;
    for (const QString &id : ids) {
        if (pnpSetEnabled(id, false))
            ++n;
        else
            failed << id;
    }

    QThread::msleep(800);
    const int left = countDesktopVirtuals();

    QString msg = QStringLiteral("已请求禁用 %1 个虚拟显示设备。").arg(n);
    if (!failed.isEmpty())
        msg += QStringLiteral("\n禁用失败: %1").arg(failed.join(QStringLiteral(", ")));
    if (left > 0)
        msg += QStringLiteral("\n警告：系统仍枚举到 %1 块虚拟屏，桌面可能仍卡；可再试一次或重启。").arg(left);
    else
        msg += QStringLiteral("\n虚拟屏已从桌面移除。");
    return msg;
}

QString VddService::applyConfig(const AppConfig &cfg, QString *detail)
{
    Q_UNUSED(detail);
    const QString gpu = preferGpuFriendlyName();
    emit progress(QStringLiteral("写入驱动配置（GPU: %1）…").arg(gpu));
    QFile f(cfg.vddSettingsPath);
    QDir().mkpath(QFileInfo(cfg.vddSettingsPath).absolutePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QStringLiteral("无法写入 %1").arg(cfg.vddSettingsPath);
    f.write(makeVddXml(cfg.displays, gpu));
    f.close();

    QVector<MonitorInfo> virtuals;
    for (const auto &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            virtuals.push_back(m);
    }

    bool reloaded = false;
    // 官方推荐：SETDISPLAYCOUNT 会写 XML count 并 reload；比盲 pnputil 稳
    if (pipeAlive()) {
        emit progress(QStringLiteral("经驱动管道设置 GPU…"));
        const QString setGpuCmd = QStringLiteral("SETGPU \"%1\"").arg(gpu);
        pipeCommand(setGpuCmd, 2000);
        QThread::msleep(1500); // SETGPU 会重启驱动，管道会短暂不可用
        emit progress(QStringLiteral("经驱动管道设置屏数 %1…").arg(cfg.displays.size()));
        // 等管道恢复
        for (int i = 0; i < 20 && !pipeAlive(); ++i)
            QThread::msleep(200);
        pipeCommand(QStringLiteral("SETDISPLAYCOUNT %1").arg(cfg.displays.size()), 2000);
        reloaded = true;
        QThread::msleep(800);
        emit progress(QStringLiteral("等待虚拟屏挂到桌面…"));
        virtuals = waitDesktopVirtuals(cfg.displays.size());
    }

    if (virtuals.size() < cfg.displays.size()) {
        emit progress(QStringLiteral("管道未就绪或屏未挂上，改用设备重启…"));
        const QStringList ids = findVddInstanceIds();
        if (ids.isEmpty())
            return QStringLiteral("未检测到 Virtual Display Driver。请先安装驱动。");
        int ok = 0;
        for (const QString &id : ids) {
            if (pnpRestart(id))
                ++ok;
        }
        if (ok == 0)
            return QStringLiteral("无法重启虚拟显示驱动设备（需要管理员权限，或设备正忙）。\n"
                                  "请关掉 Virtual Driver Control 后，在设备管理器手动禁用/启用「Virtual Display Driver」。");
        reloaded = true;
        emit progress(QStringLiteral("等待虚拟屏挂到桌面…"));
        virtuals = waitDesktopVirtuals(cfg.displays.size());
    } else if (!reloaded && virtuals.size() == cfg.displays.size()) {
        emit progress(QStringLiteral("虚拟屏已在桌面，跳过驱动重启…"));
    }

    if (virtuals.size() < cfg.displays.size())
        return diagnoseMissingVirtuals(virtuals.size(), cfg.displays.size());

    emit progress(QStringLiteral("设置分辨率与排列…"));
    MonitorInfo primary;
    bool hasPrimary = false;
    for (const auto &m : WinDisplay::listMonitors()) {
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
    for (int i = 0; i < cfg.displays.size(); ++i) {
        const DisplaySpec &spec = cfg.displays[i];
        const MonitorInfo &mon = virtuals[i];
        if (!WinDisplay::setMode(mon.deviceName, spec.width, spec.height, spec.hz, x, y))
            return QStringLiteral("设置分辨率失败: %1").arg(mon.deviceName);
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
    for (int i = 0; i < cfg.displays.size(); ++i) {
        const DisplaySpec &spec = cfg.displays[i];
        QString device = (i < virtuals.size()) ? virtuals[i].deviceName : QString();
        for (const MonitorInfo &m : after) {
            if (m.geometry.width() == spec.width && m.geometry.height() == spec.height
                && m.geometry.x() >= primary.geometry.right() - 8) {
                device = m.deviceName;
                break;
            }
        }
        if (device.isEmpty()) {
            ++dpiFail;
            continue;
        }
        if (WinDisplay::setDpiScale(device, spec.scale))
            ++dpiOk;
        else
            ++dpiFail;
    }

    QString msg = QStringLiteral("已应用 %1 块虚拟屏（GPU %2）")
                      .arg(cfg.displays.size())
                      .arg(gpu);
    if (dpiOk > 0)
        msg += QStringLiteral("，缩放成功 %1").arg(dpiOk);
    if (dpiFail > 0)
        msg += QStringLiteral("，缩放失败 %1（可到系统显示设置手动调）").arg(dpiFail);
    msg += QStringLiteral("。");
    return msg;
}
