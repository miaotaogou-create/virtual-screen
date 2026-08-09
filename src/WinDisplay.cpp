#include "WinDisplay.h"

#include <QHash>
#include <QImage>
#include <algorithm>
#include <cstring>
#include <vector>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

bool looksVirtual(const QString &text)
{
    const QString t = text.toUpper();
    return t.contains(QStringLiteral("PARSEC"))
        || t.contains(QStringLiteral("PSCCDD0"))
        || t.contains(QStringLiteral("PARSECVDA"))
        || t.contains(QStringLiteral("VIRTUAL DISPLAY"))
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

// 系统设置里观察到的缩放档位；相对「推荐值」步进
static const UINT32 kDpiSteps[] = {
    100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500
};

enum {
    DISPLAYCONFIG_DEVICE_INFO_GET_DPI_SCALE = -3,
    DISPLAYCONFIG_DEVICE_INFO_SET_DPI_SCALE = -4
};

struct SourceDpiScaleGet {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    INT32 minScaleRel;
    INT32 curScaleRel;
    INT32 maxScaleRel;
};

struct SourceDpiScaleSet {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    INT32 scaleRel;
};

UINT32 nearestDpiStep(int percent)
{
    UINT32 best = kDpiSteps[0];
    int bestDiff = qAbs(int(best) - percent);
    for (UINT32 v : kDpiSteps) {
        const int d = qAbs(int(v) - percent);
        if (d < bestDiff) {
            best = v;
            bestDiff = d;
        }
    }
    return best;
}

bool findSourceIds(const QString &deviceName, LUID *adapterId, UINT32 *sourceId)
{
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
        return false;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(),
                           &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
        return false;
    paths.resize(pathCount);

    for (const DISPLAYCONFIG_PATH_INFO &path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME srcName{};
        srcName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        srcName.header.size = sizeof(srcName);
        srcName.header.adapterId = path.sourceInfo.adapterId;
        srcName.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&srcName.header) != ERROR_SUCCESS)
            continue;
        const QString gdi = QString::fromWCharArray(srcName.viewGdiDeviceName);
        if (gdi.compare(deviceName, Qt::CaseInsensitive) == 0) {
            *adapterId = path.sourceInfo.adapterId;
            *sourceId = path.sourceInfo.id;
            return true;
        }
    }
    return false;
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

bool setDpiScale(const QString &deviceName, int scalePercent)
{
    // ponytail: DisplayConfig *-3/-4 未公开，但 Win10/11 设置页同路；档位不在表内就近取
    if (deviceName.isEmpty() || scalePercent < 100)
        return false;

    LUID adapterId{};
    UINT32 sourceId = 0;
    if (!findSourceIds(deviceName, &adapterId, &sourceId))
        return false;

    SourceDpiScaleGet getPkt{};
    getPkt.header.type = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(DISPLAYCONFIG_DEVICE_INFO_GET_DPI_SCALE);
    getPkt.header.size = sizeof(getPkt);
    getPkt.header.adapterId = adapterId;
    getPkt.header.id = sourceId;
    if (DisplayConfigGetDeviceInfo(&getPkt.header) != ERROR_SUCCESS)
        return false;

    if (getPkt.curScaleRel < getPkt.minScaleRel)
        getPkt.curScaleRel = getPkt.minScaleRel;
    if (getPkt.curScaleRel > getPkt.maxScaleRel)
        getPkt.curScaleRel = getPkt.maxScaleRel;

    const int minAbs = qAbs(int(getPkt.minScaleRel));
    const int stepCount = int(sizeof(kDpiSteps) / sizeof(kDpiSteps[0]));
    if (minAbs + getPkt.maxScaleRel + 1 > stepCount)
        return false;

    const UINT32 recommended = kDpiSteps[minAbs];
    const UINT32 minimum = kDpiSteps[minAbs + getPkt.minScaleRel];
    const UINT32 maximum = kDpiSteps[minAbs + getPkt.maxScaleRel];
    UINT32 target = nearestDpiStep(scalePercent);
    if (target < minimum)
        target = minimum;
    if (target > maximum)
        target = maximum;

    int idxTarget = -1;
    int idxReco = -1;
    for (int i = 0; i < stepCount; ++i) {
        if (kDpiSteps[i] == target)
            idxTarget = i;
        if (kDpiSteps[i] == recommended)
            idxReco = i;
    }
    if (idxTarget < 0 || idxReco < 0)
        return false;

    const INT32 rel = idxTarget - idxReco;
    if (rel == getPkt.curScaleRel)
        return true;

    SourceDpiScaleSet setPkt{};
    setPkt.header.type = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(DISPLAYCONFIG_DEVICE_INFO_SET_DPI_SCALE);
    setPkt.header.size = sizeof(setPkt);
    setPkt.header.adapterId = adapterId;
    setPkt.header.id = sourceId;
    setPkt.scaleRel = rel;
    return DisplayConfigSetDeviceInfo(&setPkt.header) == ERROR_SUCCESS;
}

QImage captureDesktopRect(const QRect &geo)
{
    if (geo.width() < 1 || geo.height() < 1)
        return {};

    const int w = geo.width();
    const int h = geo.height();
    HDC screen = GetDC(nullptr);
    if (!screen)
        return {};
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    if (!mem || !bmp) {
        if (bmp)
            DeleteObject(bmp);
        if (mem)
            DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        return {};
    }
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, screen, geo.x(), geo.y(), SRCCOPY | CAPTUREBLT);
    SelectObject(mem, old);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = -h; // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage img(w, h, QImage::Format_ARGB32);
    GetDIBits(screen, bmp, 0, UINT(h), img.bits(), reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS);

    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    // GDI 是 BGRA；Qt ARGB32 在小端同布局，可直接用
    return img;
}

} // namespace WinDisplay
