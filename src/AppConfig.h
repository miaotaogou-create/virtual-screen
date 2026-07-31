#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

struct DisplaySpec {
    QString label;
    int width = 1920;
    int height = 1080;
    int scale = 100;
    int hz = 60;
};

struct AppConfig {
    QVector<DisplaySpec> displays;
    int previewIntervalMs = 1000;
    QString vddSettingsPath = QStringLiteral("C:/VirtualDisplayDriver/vdd_settings.xml");

    static AppConfig defaults();
    static AppConfig load();
    void save() const;
    QStringList validate() const;
};

QString configPath();
