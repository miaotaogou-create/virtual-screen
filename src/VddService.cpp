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
    QStringList ids;

    // 1) pnputil：中文系统字段是「实例 ID:」；编码偶发乱码时下面有兜底
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
            // 只认描述里的 VDD 特征，不用裸 ROOT\DISPLAY 以免误伤其它设备
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
                // 兼容半角/全角冒号
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

    // 2) PowerShell 兜底：不依赖 pnputil 本地化文案
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

    // 管理员下 pnputil 偶发失败时，用 PowerShell 再试一次
    const QString cmd = enabled
        ? QStringLiteral("Enable-PnpDevice -InstanceId '%1' -Confirm:$false").arg(instanceId)
        : QStringLiteral("Disable-PnpDevice -InstanceId '%1' -Confirm:$false").arg(instanceId);
    int psCode = 0;
    runHidden(QStringLiteral("powershell"),
              {QStringLiteral("-NoProfile"), QStringLiteral("-Command"), cmd},
              &psCode);
    return psCode == 0;
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
        "装好后通常出现 C:\\VirtualDisplayDriver，再点「应用」。");
}

QString VddService::clearVirtualDisplays()
{
    // 先把监视器数量写成 0，避免下次启用又冒出旧屏
    {
        AppConfig empty = AppConfig::defaults();
        empty.displays.clear();
        QFile f(empty.vddSettingsPath);
        QDir().mkpath(QFileInfo(empty.vddSettingsPath).absolutePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            // makeVddXml 要求至少有一块时才有意义；这里手写 count=0
            f.write("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<vdd_settings>\n  <monitors>\n    <count>0</count>\n  </monitors>\n"
                    "  <global/>\n  <resolutions/>\n  <options>\n"
                    "    <CustomEdid>false</CustomEdid>\n"
                    "    <PreventSpoof>true</PreventSpoof>\n"
                    "    <HardwareCursor>true</HardwareCursor>\n"
                    "    <logging>false</logging>\n"
                    "  </options>\n</vdd_settings>\n");
        }
    }

    const QStringList ids = findVddInstanceIds();
    if (ids.isEmpty()) {
        int left = 0;
        for (const auto &m : WinDisplay::listMonitors()) {
            if (m.likelyVirtual)
                ++left;
        }
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
    int left = 0;
    for (const auto &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            ++left;
    }

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

    emit progress(QStringLiteral("设置显示缩放…"));
    QThread::msleep(400); // 等模式落定后再改 DPI，成功率更高
    int dpiOk = 0;
    int dpiFail = 0;
    // 重新枚举，按当前几何匹配刚设好的虚拟屏
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

    QString msg;
    if (nDev > 0)
        msg = QStringLiteral("已应用 %1 块虚拟屏（驱动设备 %2）").arg(cfg.displays.size()).arg(nDev);
    else
        msg = QStringLiteral("已更新 %1 块虚拟屏分辨率/位置").arg(cfg.displays.size());
    if (dpiOk > 0)
        msg += QStringLiteral("，缩放成功 %1").arg(dpiOk);
    if (dpiFail > 0)
        msg += QStringLiteral("，缩放失败 %1（可到系统显示设置手动调）").arg(dpiFail);
    msg += QStringLiteral("。");
    return msg;
}
