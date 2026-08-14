#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QTimer>

int main(int argc, char *argv[])
{
    // 尽量早声明 DPI，抓屏几何才准
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VirtualScreen"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/VirtualScreen.png")));
    QFont f = app.font();
    f.setFamily(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSize(9);
    app.setFont(f);

    // 加/删/改屏与官方一样不提权；仅装驱动需要管理员
    MainWindow w;
    w.show();

    const QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--apply")))
        QTimer::singleShot(300, &w, &MainWindow::onApply);
    else if (args.contains(QStringLiteral("--clear")))
        QTimer::singleShot(300, &w, &MainWindow::onClear);
    else if (args.contains(QStringLiteral("--install-driver")))
        QTimer::singleShot(300, &w, &MainWindow::onInstallDriver);

    return app.exec();
}
