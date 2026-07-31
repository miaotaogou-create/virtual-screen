#include "Elevate.h"
#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QTimer>

int main(int argc, char *argv[])
{
    // 尽量早声明 DPI，抓屏几何才准
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VirtualScreen"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));
    QFont f = app.font();
    f.setFamily(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSize(9);
    app.setFont(f);

    // 启动提权一次，减少后续操作反复弹 UAC
    if (Elevate::ensureAdminAtStart(argc, argv)) {
        return 0; // 已拉起管理员实例
    }

    MainWindow w;
    w.show();

    const QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--apply")))
        QTimer::singleShot(300, &w, &MainWindow::onApply);
    else if (args.contains(QStringLiteral("--clear")))
        QTimer::singleShot(300, &w, &MainWindow::onClear);

    return app.exec();
}
