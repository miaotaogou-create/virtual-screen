#pragma once

#include <QDialog>
#include <QString>

/**
 * 无边框主题提示框，风格对齐 AddDisplayDialog / PresetHubDialog。
 * 替代系统 QMessageBox，避免原生标题栏与 OK 按钮。
 */
class AppAlertDialog : public QDialog
{
    Q_OBJECT
public:
    enum Kind { Information, Warning, Question };

    static void information(QWidget *parent, const QString &title, const QString &text);
    static void warning(QWidget *parent, const QString &title, const QString &text);
    /** 确定 / 取消；返回 true 表示点了确定。 */
    static bool question(QWidget *parent, const QString &title, const QString &text,
                         const QString &okText = QString(),
                         const QString &cancelText = QString());

private:
    AppAlertDialog(Kind kind, const QString &title, const QString &text, bool showCancel,
                   const QString &okText, const QString &cancelText, QWidget *parent);
};
