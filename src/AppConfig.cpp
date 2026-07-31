#include "AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QString configPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config.json"));
}

QString profilesDir()
{
    const QString dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("profiles"));
    QDir().mkpath(dir);
    return dir;
}

static AppConfig parseObject(const QJsonObject &o)
{
    AppConfig c = AppConfig::defaults();
    c.displays.clear();
    c.previewIntervalMs = o.value(QStringLiteral("preview_interval_ms")).toInt(2000);
    c.vddSettingsPath = o.value(QStringLiteral("vdd_settings_path")).toString(c.vddSettingsPath);
    c.profileName = o.value(QStringLiteral("profile_name")).toString();
    const QJsonArray arr = o.value(QStringLiteral("displays")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject d = v.toObject();
        DisplaySpec s;
        s.label = d.value(QStringLiteral("label")).toString(QStringLiteral("屏"));
        s.width = d.value(QStringLiteral("width")).toInt(1920);
        s.height = d.value(QStringLiteral("height")).toInt(1080);
        s.scale = d.value(QStringLiteral("scale")).toInt(100);
        s.hz = d.value(QStringLiteral("hz")).toInt(60);
        c.displays.push_back(s);
    }
    if (c.displays.isEmpty())
        c = AppConfig::defaults();
    return c;
}

static QJsonObject toObject(const AppConfig &c)
{
    QJsonObject o;
    o.insert(QStringLiteral("preview_interval_ms"), c.previewIntervalMs);
    o.insert(QStringLiteral("vdd_settings_path"), c.vddSettingsPath);
    if (!c.profileName.isEmpty())
        o.insert(QStringLiteral("profile_name"), c.profileName);
    QJsonArray arr;
    for (const DisplaySpec &s : c.displays) {
        QJsonObject d;
        d.insert(QStringLiteral("label"), s.label);
        d.insert(QStringLiteral("width"), s.width);
        d.insert(QStringLiteral("height"), s.height);
        d.insert(QStringLiteral("scale"), s.scale);
        d.insert(QStringLiteral("hz"), s.hz);
        arr.append(d);
    }
    o.insert(QStringLiteral("displays"), arr);
    return o;
}

AppConfig AppConfig::defaults()
{
    AppConfig c;
    DisplaySpec a;
    a.label = QStringLiteral("虚拟屏1");
    a.width = 1920;
    a.height = 1200;
    a.scale = 100;
    a.hz = 60;
    c.displays = {a};
    c.previewIntervalMs = 2000;
    c.profileName = QStringLiteral("默认");
    return c;
}

AppConfig AppConfig::load()
{
    return loadFromFile(configPath());
}

AppConfig AppConfig::loadFromFile(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法打开: %1").arg(path);
        return defaults();
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("配置不是有效 JSON: %1").arg(path);
        return defaults();
    }
    AppConfig c = parseObject(doc.object());
    if (c.profileName.isEmpty())
        c.profileName = QFileInfo(path).completeBaseName();
    return c;
}

void AppConfig::save() const
{
    saveToFile(configPath());
}

bool AppConfig::saveToFile(const QString &path, QString *error) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("无法写入: %1").arg(path);
        return false;
    }
    f.write(QJsonDocument(toObject(*this)).toJson(QJsonDocument::Indented));
    return true;
}

QStringList AppConfig::validate() const
{
    QStringList errs;
    if (displays.isEmpty())
        errs << QStringLiteral("至少配置一块虚拟屏");
    if (displays.size() > 8)
        errs << QStringLiteral("虚拟屏数量过多（建议 ≤ 8）");
    for (int i = 0; i < displays.size(); ++i) {
        const DisplaySpec &s = displays[i];
        if (s.width < 640 || s.height < 640)
            errs << QStringLiteral("屏%1 分辨率过小").arg(i + 1);
        if (s.scale < 100)
            errs << QStringLiteral("屏%1 缩放建议 ≥ 100").arg(i + 1);
    }
    return errs;
}
