#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QTimer>

static QString loadFluentStyle()
{
    // 优先读 exe 旁 style.qss 便于热改；否则用内置资源
    const QString disk = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("style.qss"));
    QFile f(QFile::exists(disk) ? disk : QStringLiteral(":/style.qss"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VirtualScreen"));
    app.setApplicationVersion(QStringLiteral("1.1.0"));
    app.setStyleSheet(loadFluentStyle());
    app.setWindowIcon(QIcon(QStringLiteral(":/VirtualScreen.png")));
    QFont f = app.font();
    f.setFamily(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSize(9);
    app.setFont(f);

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
