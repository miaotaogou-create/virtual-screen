#include "WinDisplay.h"

#include <QHash>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

bool looksVirtual(const QString &text)
{
    const QString t = text.toUpper();
    return t.contains(QStringLiteral("VDD"))
        || t.contains(QStringLiteral("MTT"))
        || t.contains(QStringLiteral("VIRTUAL DISPLAY"))
        || t.contains(QStringLiteral("MTTVDD"))
        || t.contains(QStringLiteral("IDDSAMPLE"))
        || t.contains(QStringLiteral("INDIRECT"));
}

struct AdapterMeta {
    QString monitorName;
    QString adapterName;
    bool primary = false;
};

QHash<QString, AdapterMeta> enumAdapters()
{
    QHash<QString, AdapterMeta> map;
    for (DWORD i = 0;; ++i) {
        DISPLAY_DEVICEW adapter{};
        adapter.cb = sizeof(adapter);
        if (!EnumDisplayDevicesW(nullptr, i, &adapter, 0))
            break;
        for (DWORD j = 0;; ++j) {
            DISPLAY_DEVICEW mon{};
            mon.cb = sizeof(mon);
            if (!EnumDisplayDevicesW(adapter.DeviceName, j, &mon, 0))
                break;
            if (!(mon.StateFlags & DISPLAY_DEVICE_ACTIVE))
                continue;
            AdapterMeta m;
            m.monitorName = QString::fromWCharArray(mon.DeviceString);
            m.adapterName = QString::fromWCharArray(adapter.DeviceString);
            m.primary = (adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
            map.insert(QString::fromWCharArray(adapter.DeviceName), m);
        }
    }
    return map;
}

BOOL CALLBACK enumMonProc(HMONITOR hmon, HDC, LPRECT, LPARAM data)
{
    auto *out = reinterpret_cast<QVector<MonitorInfo> *>(data);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(hmon, &info))
        return TRUE;
    MonitorInfo m;
    m.deviceName = QString::fromWCharArray(info.szDevice);
    m.geometry = QRect(info.rcMonitor.left, info.rcMonitor.top,
                        info.rcMonitor.right - info.rcMonitor.left,
                        info.rcMonitor.bottom - info.rcMonitor.top);
    m.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    out->push_back(m);
    return TRUE;
}

} // namespace

namespace WinDisplay {

QVector<MonitorInfo> listMonitors()
{
    QVector<MonitorInfo> out;
    EnumDisplayMonitors(nullptr, nullptr, enumMonProc, reinterpret_cast<LPARAM>(&out));
    const auto adapters = enumAdapters();
    for (MonitorInfo &m : out) {
        const AdapterMeta meta = adapters.value(m.deviceName);
        m.adapterName = meta.adapterName;
        m.monitorName = meta.monitorName;
        m.likelyVirtual = looksVirtual(meta.adapterName) || looksVirtual(meta.monitorName);
        if (meta.primary)
            m.primary = true;
    }
    std::sort(out.begin(), out.end(), [](const MonitorInfo &a, const MonitorInfo &b) {
        if (a.primary != b.primary)
            return a.primary > b.primary;
        if (a.geometry.x() != b.geometry.x())
            return a.geometry.x() < b.geometry.x();
        return a.deviceName < b.deviceName;
    });
    return out;
}

QVector<MonitorInfo> previewTargets(bool preferVirtual)
{
    const auto all = listMonitors();
    QVector<MonitorInfo> virtuals;
    QVector<MonitorInfo> secondary;
    for (const MonitorInfo &m : all) {
        if (m.likelyVirtual)
            virtuals.push_back(m);
        if (!m.primary)
            secondary.push_back(m);
    }
    if (preferVirtual && !virtuals.isEmpty())
        return virtuals;
    if (!secondary.isEmpty())
        return secondary;
    return all;
}

bool setMode(const QString &deviceName, int width, int height, int hz, int x, int y)
{
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    const std::wstring name = deviceName.toStdWString();
    if (!EnumDisplaySettingsW(name.c_str(), ENUM_CURRENT_SETTINGS, &dm))
        return false;
    dm.dmPelsWidth = width;
    dm.dmPelsHeight = height;
    dm.dmDisplayFrequency = hz;
    dm.dmPosition.x = x;
    dm.dmPosition.y = y;
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_POSITION;
    const LONG rc = ChangeDisplaySettingsExW(name.c_str(), &dm, nullptr,
                                             CDS_UPDATEREGISTRY | CDS_NORESET, nullptr);
    return rc == DISP_CHANGE_SUCCESSFUL;
}

bool applyDisplayChanges()
{
    return ChangeDisplaySettingsExW(nullptr, nullptr, nullptr, 0, nullptr) == DISP_CHANGE_SUCCESSFUL;
}

bool setDpiScale(const QString &, int)
{
    // ponytail: DPI 档位因系统而异；分辨率已够用。需要时再接到 DisplayConfig。
    return false;
}

} // namespace WinDisplay
