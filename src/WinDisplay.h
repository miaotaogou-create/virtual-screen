#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

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
/** 是否存在「同一 source 绑多 target」的复制拓扑。 */
bool hasCloneTopology();
/** 强制整桌面为扩展（非复制）。 */
bool forceExtendTopology();
bool setDpiScale(const QString &deviceName, int scalePercent);
QImage captureDesktopRect(const QRect &geo);

/** 枚举可见顶层窗口（投放用）。 */
QVector<TopWindowInfo> listTopWindows();
bool moveWindowToMonitor(qulonglong hwnd, const QRect &monitorGeo);

} // namespace WinDisplay
