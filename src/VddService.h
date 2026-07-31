#pragma once

#include "AppConfig.h"

#include <QObject>
#include <QString>

class VddService : public QObject
{
    Q_OBJECT
public:
    explicit VddService(QObject *parent = nullptr);

    bool driverReady() const;
    QString applyConfig(const AppConfig &cfg, QString *detail = nullptr);
    QString clearVirtualDisplays();
    QString installDriverHint() const;

signals:
    void progress(const QString &msg);
};
