#pragma once

#include <QRect>
#include <QString>
#include <QVector>
#include <QImage>

struct MonitorInfo {
    QString deviceName;   // \\.\DISPLAYn
    QString adapterName;
    QString monitorName;
    QRect geometry;       // 虚拟桌面坐标
    bool primary = false;
    bool likelyVirtual = false;
};

namespace WinDisplay {

QVector<MonitorInfo> listMonitors();
QVector<MonitorInfo> previewTargets(bool preferVirtual = true);

bool setMode(const QString &deviceName, int width, int height, int hz, int x, int y);
bool applyDisplayChanges();
/** 尽力设置缩放百分比；失败不抛，返回 false。 */
bool setDpiScale(const QString &deviceName, int scalePercent);

/** 用 GDI 抓取桌面矩形，可在工作线程调用（返回 QImage）。 */
QImage captureDesktopRect(const QRect &geo);

} // namespace WinDisplay
