#pragma once

#include <QString>
#include <QStringList>
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
    int previewIntervalMs = 2000;
    QString vddSettingsPath = QStringLiteral("C:/VirtualDisplayDriver/vdd_settings.xml");
    /** 当前配置文件名（不含路径），仅作界面提示。 */
    QString profileName;

    static AppConfig defaults();
    static AppConfig load();
    static AppConfig loadFromFile(const QString &path, QString *error = nullptr);
    void save() const;
    bool saveToFile(const QString &path, QString *error = nullptr) const;
    QStringList validate() const;
};

QString configPath();
QString profilesDir();
QStringList listProfilePaths();
bool deleteProfileFile(const QString &path, QString *error = nullptr);