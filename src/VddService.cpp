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

QVector<MonitorInfo> currentVirtuals()
{
    QVector<MonitorInfo> virtuals;
    for (const MonitorInfo &m : WinDisplay::listMonitors()) {
        if (m.likelyVirtual)
            virtuals.push_back(m);
    }
    return virtuals;
}

} // namespace

VddService::VddService(QObject *parent)
    : QObject(parent)
{
    m_ping = new QTimer(this);
    m_ping->setInterval(80);
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
    // 保活定时器必须在对象所在线程（主线程）启动
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { startPing(); }, Qt::QueuedConnection);
        return;
    }
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
    return writeParsecCustomModes(cfg.displays, err);
}

bool VddService::writeParsecCustomModes(const QVector<DisplaySpec> &displays, QString *err)
{
    struct Mode {
        int w, h, hz;
    };
    QVector<Mode> modes;
    QSet<QString> seen;
    for (const DisplaySpec &d : displays) {
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
                *err = QStringLiteral("写入自定义分辨率失败（需要管理员权限）");
            return false;
        }
    }
    return true;
}

int VddService::countDesktopVirtuals() const
{
    return countLikelyVirtuals();
}

QString VddService::arrangeAndDpi(const AppConfig &cfg)
{
    const int want = cfg.displays.size();
    QVector<MonitorInfo> virtuals = waitVirtuals(want);
    if (virtuals.size() < want) {
        return QStringLiteral("已请求 %1 块虚拟屏，但桌面只看到 %2 块。\n"
                              "请到「显示设置」确认是「扩展」而不是「复制」。")
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
        if (!WinDisplay::setMode(virtuals[i].deviceName, spec.width, spec.height, spec.hz, x, y))
            return QStringLiteral("设置分辨率失败: %1").arg(virtuals[i].deviceName);
        x += spec.width;
    }
    WinDisplay::applyDisplayChanges();

    emit progress(QStringLiteral("设置显示缩放…"));
    QThread::msleep(400);
    int dpiOk = 0;
    int dpiFail = 0;
    const QVector<MonitorInfo> after = currentVirtuals();
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

    QString msg = QStringLiteral("已就绪 %1 块虚拟屏（Parsec）").arg(want);
    if (dpiOk > 0)
        msg += QStringLiteral("，缩放成功 %1").arg(dpiOk);
    if (dpiFail > 0)
        msg += QStringLiteral("，缩放失败 %1").arg(dpiFail);
    return msg;
}

QString VddService::applyConfig(const AppConfig &cfg, QString *detail)
{
    Q_UNUSED(detail);
    QString err;
    if (!ensureParsecOpen(&err))
        return err;

    // 自定义模式写 HKLM 需要管理员；失败不阻断——驱动自带常见分辨率
    QString customErr;
    const bool customOk = writeParsecCustomModes(cfg, &customErr);

    const int want = cfg.displays.size();
    if (want > VDD_MAX_DISPLAYS)
        return QStringLiteral("最多 %1 块虚拟屏。").arg(VDD_MAX_DISPLAYS);

    emit progress(QStringLiteral("重建虚拟屏…"));
    for (int i = m_parsecIndices.size() - 1; i >= 0; --i) {
        VddRemoveDisplay(static_cast<HANDLE>(m_parsec), m_parsecIndices[i]);
        QThread::msleep(120);
    }
    m_parsecIndices.clear();
    QThread::msleep(300);

    int onDesktop = countDesktopVirtuals();
    for (int idx = onDesktop - 1; idx >= 0 && onDesktop > 0; --idx) {
        VddRemoveDisplay(static_cast<HANDLE>(m_parsec), idx);
        QThread::msleep(120);
        onDesktop = countDesktopVirtuals();
    }
    QThread::msleep(250);

    for (int i = 0; i < want; ++i) {
        const int idx = VddAddDisplay(static_cast<HANDLE>(m_parsec));
        if (idx < 0)
            return QStringLiteral("添加第 %1 块虚拟屏失败。").arg(i + 1);
        m_parsecIndices.push_back(idx);
        QThread::msleep(180);
    }

    QString msg = arrangeAndDpi(cfg);
    if (!msg.startsWith(QStringLiteral("已就绪")) && !msg.startsWith(QStringLiteral("已应用")))
        return msg;
    // 统一前缀，方便 UI 判成功
    if (!msg.startsWith(QStringLiteral("已应用")))
        msg = QStringLiteral("已应用。") + msg;
    if (!customOk)
        msg += QStringLiteral("\n提示：自定义分辨率未写入注册表（%1）。常用分辨率一般仍可用。")
                   .arg(customErr);
    msg += QStringLiteral("\n请保持本程序运行以维持虚拟屏。");
    return msg;
}

QString VddService::clearVirtualDisplays()
{
    QString err;
    if (!ensureParsecOpen(&err))
        return QStringLiteral("未找到 Parsec VDD，无需清除。");

    emit progress(QStringLiteral("移除虚拟屏…"));
    for (int i = m_parsecIndices.size() - 1; i >= 0; --i) {
        VddRemoveDisplay(static_cast<HANDLE>(m_parsec), m_parsecIndices[i]);
        QThread::msleep(100);
    }
    m_parsecIndices.clear();

    int left = countDesktopVirtuals();
    for (int idx = left - 1; idx >= 0; --idx) {
        VddRemoveDisplay(static_cast<HANDLE>(m_parsec), idx);
        QThread::msleep(100);
    }
    QThread::msleep(400);
    left = countDesktopVirtuals();
    closeParsec();

    if (left > 0)
        return QStringLiteral("已请求禁用，但桌面仍看到 %1 块虚拟屏。").arg(left);
    return QStringLiteral("已请求禁用虚拟屏，虚拟屏已从桌面移除。");
}

QString VddService::addOne(const DisplaySpec &spec)
{
    QString err;
    if (!ensureParsecOpen(&err))
        return err;
    if (m_parsecIndices.size() >= VDD_MAX_DISPLAYS)
        return QStringLiteral("最多 %1 块虚拟屏。").arg(VDD_MAX_DISPLAYS);

    // 与官方一致：加屏只走驱动 IOCTL，不写 HKLM、不提权
    const int idx = VddAddDisplay(static_cast<HANDLE>(m_parsec));
    if (idx < 0)
        return QStringLiteral("添加虚拟屏失败。");
    m_parsecIndices.push_back(idx);
    QThread::msleep(280);

    QVector<MonitorInfo> virtuals = waitVirtuals(m_parsecIndices.size());
    if (virtuals.isEmpty())
        return QStringLiteral("已添加，但桌面尚未枚举到虚拟屏（可点刷新）。");

    const MonitorInfo &mon = virtuals.last();
    MonitorInfo primary;
    for (const MonitorInfo &m : WinDisplay::listMonitors()) {
        if (m.primary) {
            primary = m;
            break;
        }
    }
    const int x = primary.deviceName.isEmpty() ? mon.geometry.x() : primary.geometry.right();
    const int y = primary.deviceName.isEmpty() ? mon.geometry.y() : primary.geometry.top();
    // 模式设置失败也不回滚加屏——驱动默认模式仍可用（与官方体验一致）
    if (!WinDisplay::setMode(mon.deviceName, spec.width, spec.height, spec.hz, x, y)) {
        WinDisplay::applyDisplayChanges();
        return QStringLiteral("已添加「%1」（默认模式；%2×%3 可能需在右键改，或管理员写自定义模式）")
            .arg(spec.label)
            .arg(spec.width)
            .arg(spec.height);
    }
    WinDisplay::applyDisplayChanges();
    QThread::msleep(250);
    WinDisplay::setDpiScale(mon.deviceName, spec.scale); // 失败忽略
    return QStringLiteral("已添加「%1」%2×%3。").arg(spec.label).arg(spec.width).arg(spec.height);
}

QString VddService::removeAt(int index)
{
    if (index < 0 || index >= m_parsecIndices.size())
        return QStringLiteral("无效的虚拟屏索引。");
    QString err;
    if (!ensureParsecOpen(&err))
        return err;

    const int driverIndex = m_parsecIndices[index];
    VddRemoveDisplay(static_cast<HANDLE>(m_parsec), driverIndex);
    m_parsecIndices.removeAt(index);
    QThread::msleep(250);
    return QStringLiteral("已删除虚拟屏。");
}

QString VddService::updateAt(int index, const DisplaySpec &spec, const QVector<DisplaySpec> &allDisplays)
{
    if (index < 0)
        return QStringLiteral("无效的虚拟屏索引。");
    QString err;
    if (!ensureParsecOpen(&err))
        return err;

    QVector<MonitorInfo> virtuals = currentVirtuals();
    if (index >= virtuals.size())
        virtuals = waitVirtuals(index + 1);
    if (index >= virtuals.size())
        return QStringLiteral("找不到第 %1 块虚拟屏。").arg(index + 1);

    const MonitorInfo &mon = virtuals[index];
    if (!WinDisplay::setMode(mon.deviceName, spec.width, spec.height, spec.hz,
                             mon.geometry.x(), mon.geometry.y())) {
        // 驱动模式表没有该分辨率时，才尝试写自定义（要管理员）
        QVector<DisplaySpec> modes = allDisplays;
        if (index < modes.size())
            modes[index] = spec;
        else
            modes.push_back(spec);
        QString regErr;
        if (!writeParsecCustomModes(modes, &regErr)) {
            return QStringLiteral("设置分辨率失败（%1×%2 @%3）。\n"
                                  "该模式可能不在驱动表中；写自定义模式需要管理员：%4")
                .arg(spec.width)
                .arg(spec.height)
                .arg(spec.hz)
                .arg(regErr);
        }
        // 自定义模式通常需重新插拔才进表；先再试一次 setMode
        if (!WinDisplay::setMode(mon.deviceName, spec.width, spec.height, spec.hz,
                                 mon.geometry.x(), mon.geometry.y())) {
            return QStringLiteral("已写入自定义模式，但尚未生效。\n"
                                  "请删除该虚拟屏再添加一次，或重启本程序后重试。");
        }
    }
    WinDisplay::applyDisplayChanges();
    QThread::msleep(250);
    if (!WinDisplay::setDpiScale(mon.deviceName, spec.scale)) {
        return QStringLiteral("已更新为 %1×%2 @%3Hz；缩放未改成（可到系统显示设置手动调）。")
            .arg(spec.width)
            .arg(spec.height)
            .arg(spec.hz);
    }
    return QStringLiteral("已更新为 %1×%2 @%3Hz 缩放%4%。")
        .arg(spec.width)
        .arg(spec.height)
        .arg(spec.hz)
        .arg(spec.scale);
}
