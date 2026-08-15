#pragma once

#include "AppConfig.h"

#include <QWidget>

class QButtonGroup;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class SwitchButton;

class PropertiesDrawer : public QWidget
{
    Q_OBJECT
public:
    explicit PropertiesDrawer(QWidget *parent = nullptr);

    void loadDisplay(int index, const DisplaySpec &spec, bool hasVirtual);
    void setEnabledDrawer(bool on);
    void setCapturedApp(const QString &title, const QString &exe);
    void clearCapturedApp();

signals:
    void displayEdited(int index, const DisplaySpec &spec);
    void removeRequested(int index);
    void castRequested(int index);
    void castDetachRequested(int index);
    void closeRequested();

private:
    void setupUi();
    void scheduleEmit();
    void emitNow();
    DisplaySpec readForm() const;
    void updateValueLabels();
    void syncPresetComboFromSize();
    void selectHz(int hz);
    void selectScale(int scale);
    void updateCastVisibility();

    int m_index = -1;
    bool m_hasVirtual = false;
    bool m_block = false;

    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_presetCombo = nullptr;
    QLabel *m_presetValue = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QLabel *m_hzValue = nullptr;
    QLabel *m_scaleValue = nullptr;
    QButtonGroup *m_hzGroup = nullptr;
    QButtonGroup *m_scaleGroup = nullptr;
    QPushButton *m_landscapeBtn = nullptr;
    QPushButton *m_portraitBtn = nullptr;
    SwitchButton *m_hdrSwitch = nullptr;
    SwitchButton *m_primarySwitch = nullptr;
    QPushButton *m_castNewBtn = nullptr;
    QFrame *m_castEmpty = nullptr;
    QFrame *m_castAppCard = nullptr;
    QLabel *m_castAppTitle = nullptr;
    QLabel *m_castAppExe = nullptr;
    QPushButton *m_castDetachBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QTimer *m_debounce = nullptr;
};
