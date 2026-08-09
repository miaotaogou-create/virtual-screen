#include "WinDisplay.h"

#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QtGlobal>
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
    return captureDesktopRect(geo, QSize());
}

QImage captureDesktopRect(const QRect &geo, const QSize &maxSize)
{
    if (geo.width() < 1 || geo.height() < 1)
        return {};

    const int srcW = geo.width();
    const int srcH = geo.height();
    int dstW = srcW;
    int dstH = srcH;
    if (maxSize.width() > 1 && maxSize.height() > 1
        && (srcW > maxSize.width() || srcH > maxSize.height())) {
        const QSize fitted = QSize(srcW, srcH).scaled(maxSize, Qt::KeepAspectRatio);
        dstW = fitted.width();
        dstH = fitted.height();
    }

    HDC screen = GetDC(nullptr);
    if (!screen)
        return {};
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, dstW, dstH);
    if (!mem || !bmp) {
        if (bmp)
            DeleteObject(bmp);
        if (mem)
            DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        return {};
    }
    HGDIOBJ old = SelectObject(mem, bmp);
    if (dstW == srcW && dstH == srcH) {
        BitBlt(mem, 0, 0, dstW, dstH, screen, geo.x(), geo.y(), SRCCOPY | CAPTUREBLT);
    } else {
        SetStretchBltMode(mem, COLORONCOLOR);
        StretchBlt(mem, 0, 0, dstW, dstH, screen, geo.x(), geo.y(), srcW, srcH, SRCCOPY | CAPTUREBLT);
    }
    SelectObject(mem, old);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = dstW;
    bi.biHeight = -dstH;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage img(dstW, dstH, QImage::Format_ARGB32);
    GetDIBits(screen, bmp, 0, UINT(dstH), img.bits(), reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS);

    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return img;
}

QVector<TopWindowInfo> listTopWindows()
{
    QVector<TopWindowInfo> out;
    struct Ctx {
        QVector<TopWindowInfo> *out;
    } ctx{&out};

    auto proc = [](HWND hwnd, LPARAM lp) -> BOOL {
        auto *c = reinterpret_cast<Ctx *>(lp);
        if (!IsWindowVisible(hwnd))
            return TRUE;
        if (GetWindow(hwnd, GW_OWNER))
            return TRUE;
        const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        if (!(style & WS_VISIBLE))
            return TRUE;
        wchar_t title[512];
        const int n = GetWindowTextW(hwnd, title, 512);
        if (n <= 0)
            return TRUE;
        // 跳过工具窗 / 无标题
        const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (ex & WS_EX_TOOLWINDOW)
            return TRUE;

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        QString procName;
        if (pid) {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (h) {
                wchar_t path[MAX_PATH];
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(h, 0, path, &size))
                    procName = QFileInfo(QString::fromWCharArray(path)).fileName();
                CloseHandle(h);
            }
        }
        const QString t = QString::fromWCharArray(title);
        if (t.contains(QStringLiteral("VirtualScreen")))
            return TRUE;

        TopWindowInfo info;
        info.hwnd = reinterpret_cast<qulonglong>(hwnd);
        info.title = t;
        info.processName = procName;
        c->out->push_back(info);
        return TRUE;
    };

    EnumWindows(proc, reinterpret_cast<LPARAM>(&ctx));
    return out;
}

bool moveWindowToMonitor(qulonglong hwndVal, const QRect &monitorGeo)
{
    HWND hwnd = reinterpret_cast<HWND>(hwndVal);
    if (!hwnd || !IsWindow(hwnd))
        return false;
    ShowWindow(hwnd, SW_RESTORE);
    // 留一点边距，避免贴边被当成最大化异常
    const int x = monitorGeo.x() + 8;
    const int y = monitorGeo.y() + 8;
    const int w = qMax(320, monitorGeo.width() - 16);
    const int h = qMax(240, monitorGeo.height() - 16);
    const BOOL ok = SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
    if (ok)
        ShowWindow(hwnd, SW_MAXIMIZE);
    return ok == TRUE;
}

static LONG toAbsoluteX(int desktopX, int vx, int vw)
{
    return MulDiv(desktopX - vx, 65535, qMax(1, vw - 1));
}

static LONG toAbsoluteY(int desktopY, int vy, int vh)
{
    return MulDiv(desktopY - vy, 65535, qMax(1, vh - 1));
}

bool sendMouseAt(int desktopX, int desktopY, Qt::MouseButton button, bool pressed, int wheelDelta)
{
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw <= 1 || vh <= 1)
        return false;

    // 注入后立刻把光标拉回，系统指针不留在虚拟屏上；预览里靠软光标显示位置
    POINT old{};
    GetCursorPos(&old);

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = toAbsoluteX(desktopX, vx, vw);
    in.mi.dy = toAbsoluteY(desktopY, vy, vh);
    in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE;

    if (wheelDelta != 0) {
        in.mi.dwFlags |= MOUSEEVENTF_WHEEL;
        in.mi.mouseData = DWORD(wheelDelta);
        const UINT n = SendInput(1, &in, sizeof(INPUT));
        SetCursorPos(old.x, old.y);
        return n == 1;
    }

    if (button == Qt::LeftButton)
        in.mi.dwFlags |= pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    else if (button == Qt::RightButton)
        in.mi.dwFlags |= pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    else if (button == Qt::MiddleButton)
        in.mi.dwFlags |= pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;

    const UINT n = SendInput(1, &in, sizeof(INPUT));
    SetCursorPos(old.x, old.y);
    return n == 1;
}

static WORD vkFromQtKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return WORD('A' + (key - Qt::Key_A));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return WORD('0' + (key - Qt::Key_0));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
        return WORD(VK_F1 + (key - Qt::Key_F1));
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VK_RETURN;
    case Qt::Key_Escape:
        return VK_ESCAPE;
    case Qt::Key_Tab:
        return VK_TAB;
    case Qt::Key_Backspace:
        return VK_BACK;
    case Qt::Key_Delete:
        return VK_DELETE;
    case Qt::Key_Insert:
        return VK_INSERT;
    case Qt::Key_Home:
        return VK_HOME;
    case Qt::Key_End:
        return VK_END;
    case Qt::Key_PageUp:
        return VK_PRIOR;
    case Qt::Key_PageDown:
        return VK_NEXT;
    case Qt::Key_Left:
        return VK_LEFT;
    case Qt::Key_Right:
        return VK_RIGHT;
    case Qt::Key_Up:
        return VK_UP;
    case Qt::Key_Down:
        return VK_DOWN;
    case Qt::Key_Space:
        return VK_SPACE;
    case Qt::Key_Shift:
        return VK_SHIFT;
    case Qt::Key_Control:
        return VK_CONTROL;
    case Qt::Key_Alt:
        return VK_MENU;
    default:
        return 0;
    }
}

bool sendKey(int qtKey, Qt::KeyboardModifiers mods, bool pressed)
{
    Q_UNUSED(mods);
    const WORD vk = vkFromQtKey(qtKey);
    if (!vk)
        return false;
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

} // namespace WinDisplay
