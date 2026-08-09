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

    emit progress(QStringLiteral("写入自定义分辨率…"));
    if (!writeParsecCustomModes(cfg, &err))
        return err;

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

    return arrangeAndDpi(cfg) + QStringLiteral("。请保持本程序运行以维持虚拟屏。");
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
        return QStringLiteral("已请求清除，但桌面仍看到 %1 块虚拟屏。").arg(left);
    return QStringLiteral("已请求禁用虚拟屏，虚拟屏已从桌面移除。");
}

QString VddService::addOne(const DisplaySpec &spec)
{
    QString err;
    if (!ensureParsecOpen(&err))
        return err;
    if (m_parsecIndices.size() >= VDD_MAX_DISPLAYS)
        return QStringLiteral("最多 %1 块虚拟屏。").arg(VDD_MAX_DISPLAYS);

    QVector<DisplaySpec> all;
    // 合并已有模式 + 新模式写入注册表
    for (const MonitorInfo &m : currentVirtuals()) {
        DisplaySpec d;
        d.width = m.geometry.width();
        d.height = m.geometry.height();
        d.hz = 60;
        all.push_back(d);
    }
    all.push_back(spec);
    if (!writeParsecCustomModes(all, &err))
        return err;

    const int idx = VddAddDisplay(static_cast<HANDLE>(m_parsec));
    if (idx < 0)
        return QStringLiteral("添加虚拟屏失败。");
    m_parsecIndices.push_back(idx);
    QThread::msleep(300);

    AppConfig tmp;
    // 用当前桌面虚拟屏数量构造临时配置做排列
    QVector<MonitorInfo> virtuals = waitVirtuals(m_parsecIndices.size());
    for (int i = 0; i < virtuals.size(); ++i) {
        DisplaySpec d;
        d.label = QStringLiteral("虚拟屏%1").arg(i + 1);
        if (i + 1 == m_parsecIndices.size()) {
            d = spec;
            if (d.label.trimmed().isEmpty())
                d.label = QStringLiteral("虚拟屏%1").arg(i + 1);
        } else {
            d.width = virtuals[i].geometry.width();
            d.height = virtuals[i].geometry.height();
            d.hz = 60;
            d.scale = 100;
        }
        tmp.displays.push_back(d);
    }
    // 最后一块强制用用户 spec
    if (!tmp.displays.isEmpty())
        tmp.displays.last() = spec;

    const QString msg = arrangeAndDpi(tmp);
    if (msg.contains(QStringLiteral("失败")) || msg.contains(QStringLiteral("只看到")))
        return msg;
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

    QVector<DisplaySpec> modes = allDisplays;
    if (index < modes.size())
        modes[index] = spec;
    else
        modes.push_back(spec);
    if (!writeParsecCustomModes(modes, &err))
        return err;

    QVector<MonitorInfo> virtuals = currentVirtuals();
    if (index >= virtuals.size())
        virtuals = waitVirtuals(index + 1);
    if (index >= virtuals.size())
        return QStringLiteral("找不到第 %1 块虚拟屏。").arg(index + 1);

    const MonitorInfo &mon = virtuals[index];
    if (!WinDisplay::setMode(mon.deviceName, spec.width, spec.height, spec.hz,
                             mon.geometry.x(), mon.geometry.y()))
        return QStringLiteral("设置分辨率失败。");
    WinDisplay::applyDisplayChanges();
    QThread::msleep(300);
    if (!WinDisplay::setDpiScale(mon.deviceName, spec.scale))
        return QStringLiteral("已改分辨率，但缩放设置失败（可到系统显示设置手动调）。");
    return QStringLiteral("已更新为 %1×%2 @%3Hz 缩放%4%。")
        .arg(spec.width)
        .arg(spec.height)
        .arg(spec.hz)
        .arg(spec.scale);
}
