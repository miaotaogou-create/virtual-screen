#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>
#include <Qt>

struct MonitorInfo {
    QString deviceName;
    QString adapterName;
    QString monitorName;
    QRect geometry;
    bool primary = false;
    bool likelyVirtual = false;
};

struct TopWindowInfo {
    qulonglong hwnd = 0;
    QString title;
    QString processName;
};

namespace WinDisplay {

QVector<MonitorInfo> listMonitors();
QVector<MonitorInfo> previewTargets(bool preferVirtual = true);

bool setMode(const QString &deviceName, int width, int height, int hz, int x, int y);
bool applyDisplayChanges();
bool setDpiScale(const QString &deviceName, int scalePercent);
QImage captureDesktopRect(const QRect &geo);
/** 直接缩放到 maxSize 内（预览用，更快）。 */
QImage captureDesktopRect(const QRect &geo, const QSize &maxSize);

/** 枚举可见顶层窗口（投放用）。 */
QVector<TopWindowInfo> listTopWindows();
bool moveWindowToMonitor(qulonglong hwnd, const QRect &monitorGeo);

/** 向虚拟桌面绝对坐标注入鼠标（含跨屏）。 */
bool sendMouseAt(int desktopX, int desktopY, Qt::MouseButton button, bool pressed, int wheelDelta = 0);
bool sendKey(int qtKey, Qt::KeyboardModifiers mods, bool pressed);

} // namespace WinDisplay
