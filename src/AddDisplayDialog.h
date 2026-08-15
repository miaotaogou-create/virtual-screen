#pragma once

#include "AppConfig.h"

#include <QDialog>

class QButtonGroup;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;

class AddDisplayDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddDisplayDialog(int ordinal, QWidget *parent = nullptr);

    DisplaySpec resultSpec() const { return m_spec; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void applyTemplate(int index);
    void syncFromForm();
    void updateResolutionLabel();
    void updateHdrRow(bool on);

    DisplaySpec m_spec;
    int m_ordinal = 1;
    int m_width = 1920;
    int m_height = 1080;
    bool m_hdr = true;

    QButtonGroup *m_tplGroup = nullptr;
    QPushButton *m_tpl1080 = nullptr;
    QPushButton *m_tpl2k = nullptr;
    QPushButton *m_tpl4k = nullptr;
    QLabel *m_resValue = nullptr;
    QComboBox *m_hz = nullptr;
    QFrame *m_hdrRow = nullptr;
    QLabel *m_hdrStatus = nullptr;
};
