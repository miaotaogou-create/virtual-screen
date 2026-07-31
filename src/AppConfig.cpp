#include "AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QString configPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config.json"));
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
    DisplaySpec b = a;
    b.label = QStringLiteral("虚拟屏2");
    c.displays = {a, b};
    return c;
}

AppConfig AppConfig::load()
{
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly))
        return defaults();
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return defaults();
    const QJsonObject o = doc.object();
    AppConfig c = defaults();
    c.previewIntervalMs = o.value(QStringLiteral("preview_interval_ms")).toInt(1000);
    c.vddSettingsPath = o.value(QStringLiteral("vdd_settings_path")).toString(c.vddSettingsPath);
    const QJsonArray arr = o.value(QStringLiteral("displays")).toArray();
    if (!arr.isEmpty()) {
        c.displays.clear();
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
    }
    return c;
}

void AppConfig::save() const
{
    QJsonObject o;
    o.insert(QStringLiteral("preview_interval_ms"), previewIntervalMs);
    o.insert(QStringLiteral("vdd_settings_path"), vddSettingsPath);
    QJsonArray arr;
    for (const DisplaySpec &s : displays) {
        QJsonObject d;
        d.insert(QStringLiteral("label"), s.label);
        d.insert(QStringLiteral("width"), s.width);
        d.insert(QStringLiteral("height"), s.height);
        d.insert(QStringLiteral("scale"), s.scale);
        d.insert(QStringLiteral("hz"), s.hz);
        arr.append(d);
    }
    o.insert(QStringLiteral("displays"), arr);
    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

QStringList AppConfig::validate() const
{
    QStringList errs;
    if (displays.isEmpty())
        errs << QStringLiteral("至少配置一块虚拟屏");
    for (int i = 0; i < displays.size(); ++i) {
        const DisplaySpec &s = displays[i];
        if (s.width < 640 || s.height < 640)
            errs << QStringLiteral("屏%1 分辨率过小").arg(i + 1);
        if (s.scale < 100)
            errs << QStringLiteral("屏%1 缩放建议 ≥ 100").arg(i + 1);
    }
    return errs;
}
