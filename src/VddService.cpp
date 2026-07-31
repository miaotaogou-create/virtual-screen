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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef CREATE_NO_WINDOW
#define CREATE_NO_WINDOW 0x08000000
#endif

namespace {

QByteArray makeVddXml(const QVector<DisplaySpec> &displays)
{
    QString xml;
    QTextStream ts(&xml);
    ts << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    ts << "<vdd_settings>\n  <monitors>\n    <count>" << displays.size() << "</count>\n  </monitors>\n";
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
    ts << "  </resolutions>\n"
       << "  <options>\n"
       << "    <CustomEdid>false</CustomEdid>\n"
       << "    <PreventSpoof>true</PreventSpoof>\n"
       << "    <HardwareCursor>true</HardwareCursor>\n"
       << "    <logging>false</logging>\n"
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
    // 隐藏控制台
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
    int code = 0;
    const QString out = runHidden(QStringLiteral("pnputil"),
                                  {QStringLiteral("/enum-devices"), QStringLiteral("/connected")},
                                  &code);
    QStringList ids;
    QString curId;
    QString curDesc;
    const QStringList lines = out.split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);
    auto flush = [&]() {
        const QString blob = (curDesc + QLatin1Char(' ') + curId).toLower();
        if (blob.contains(QStringLiteral("virtual display"))
            || blob.contains(QStringLiteral("mttvdd"))
            || blob.contains(QStringLiteral("iddsample"))
            || blob.contains(QStringLiteral("indirect display"))
            || blob.contains(QStringLiteral("vdd by"))) {
            if (!curId.isEmpty())
                ids << curId;
        }
        curId.clear();
        curDesc.clear();
    };
    for (QString line : lines) {
        line = line.trimmed();
        const QString low = line.toLower();
        if (low.startsWith(QStringLiteral("instance id:")) || low.contains(QStringLiteral("instance id:"))) {
            flush();
            curId = line.section(QLatin1Char(':'), 1).trimmed();
        } else if (low.startsWith(QStringLiteral("device description:"))
                   || low.startsWith(QStringLiteral("设备描述:"))) {
            curDesc = line.section(QLatin1Char(':'), 1).trimmed();
        }
    }
    flush();
    return ids;
}

bool pnpSetEnabled(const QString &instanceId, bool enabled)
{
    int code = 0;
    runHidden(QStringLiteral("pnputil"),
              {enabled ? QStringLiteral("/enable-device") : QStringLiteral("/disable-device"), instanceId},
              &code);
    return code == 0;
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
        "请先安装 Virtual Display Driver（官方包或本工具旧版「安装驱动」）。"
        "安装后目录通常为 C:\\VirtualDisplayDriver。");
}

QString VddService::clearVirtualDisplays()
{
    const QStringList ids = findVddInstanceIds();
    int n = 0;
    for (const QString &id : ids) {
        if (pnpSetEnabled(id, false))
            ++n;
    }
    if (n == 0)
        return QStringLiteral("未找到可禁用的虚拟显示设备。");
    return QStringLiteral("已禁用 %1 个虚拟显示设备。").arg(n);
}

QString VddService::applyConfig(const AppConfig &cfg, QString *detail)
{
    Q_UNUSED(detail);
    emit progress(QStringLiteral("写入驱动配置…"));
    QFile f(cfg.vddSettingsPath);
    QDir().mkpath(QFileInfo(cfg.vddSettingsPath).absolutePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QStringLiteral("无法写入 %1").arg(cfg.vddSettingsPath);
    f.write(makeVddXml(cfg.displays));
    f.close();

    auto existing = WinDisplay::listMonitors();
    QVector<MonitorInfo> virtuals;
    for (const auto &m : existing) {
        if (m.likelyVirtual)
            virtuals.push_back(m);
    }

    int nDev = 0;
    // 数量变了必须重启，否则多出来的屏会残留、少了则起不来
    if (virtuals.size() == cfg.displays.size()) {
        emit progress(QStringLiteral("虚拟屏已在线，跳过驱动重启…"));
    } else {
        emit progress(QStringLiteral("重启虚拟显示驱动…"));
        const QStringList ids = findVddInstanceIds();
        if (ids.isEmpty())
            return QStringLiteral("未检测到 Virtual Display Driver。请先安装驱动。");
        for (const QString &id : ids) {
            pnpSetEnabled(id, false);
            QThread::msleep(250);
            pnpSetEnabled(id, true);
            ++nDev;
        }
        QThread::msleep(600);
        emit progress(QStringLiteral("等待虚拟屏出现…"));
        for (int i = 0; i < 40; ++i) {
            virtuals.clear();
            for (const auto &m : WinDisplay::listMonitors()) {
                if (m.likelyVirtual)
                    virtuals.push_back(m);
            }
            if (virtuals.size() >= cfg.displays.size())
                break;
            QThread::msleep(200);
        }
        virtuals = virtuals.mid(0, cfg.displays.size());
    }

    if (virtuals.size() < cfg.displays.size()) {
        return QStringLiteral("已写入配置，但只看到 %1 块虚拟屏（期望 %2）。")
            .arg(virtuals.size())
            .arg(cfg.displays.size());
    }

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

    if (nDev > 0)
        return QStringLiteral("已应用 %1 块虚拟屏（驱动设备 %2）。").arg(cfg.displays.size()).arg(nDev);
    return QStringLiteral("已更新 %1 块虚拟屏分辨率/位置。").arg(cfg.displays.size());
}
