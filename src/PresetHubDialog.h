#pragma once

#include "AppConfig.h"

#include <QDialog>

class PresetHubDialog : public QDialog
{
    Q_OBJECT
public:
    enum Action { None, LoadProfile, SaveAsNew };

    PresetHubDialog(const AppConfig &current, const QString &activeProfilePath,
                    QWidget *parent = nullptr);

    Action action() const { return m_action; }
    QString selectedPath() const { return m_selectedPath; }
    bool profilesChanged() const { return m_profilesChanged; }

private:
    void rebuildList();
    void onLoad(const QString &path);
    void onSaveNew();
    void onDeleteProfile(const QString &path);
    void onExportJson();

    AppConfig m_current;
    QString m_activePath;
    Action m_action = None;
    QString m_selectedPath;
    bool m_profilesChanged = false;

    class QVBoxLayout *m_listLayout = nullptr;
};
