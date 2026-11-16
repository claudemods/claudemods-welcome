#include "backend.h"
#include "cli.h"
#include "externals.h"
#include "logger.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QUrl>

int main(int argc, char *argv[])
{
    logger::setup();

    // Command line mode: `claudemods-hello fix|tweak|dns|launch ...`
    if (argc > 1) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QString::fromLatin1(ext::kBinaryName));
        QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
        Backend::setupLocalization();
        return cli::run(app.arguments());
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QString::fromLatin1(ext::kBinaryName));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QGuiApplication::setOrganizationName(QStringLiteral("claudemods"));
    QGuiApplication::setDesktopFileName(QString::fromLatin1(ext::kBinaryName));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/org.claudemods.hello.svg")));

    // Single instance: a second launch just raises the running window.
    const QString serverName = QStringLiteral("claudemods-hello-%1").arg(qEnvironmentVariable("USER"));
    {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(300)) {
            probe.write("raise");
            probe.waitForBytesWritten(300);
            return 0;
        }
    }
    QLocalServer::removeServer(serverName);
    QLocalServer instanceServer;
    instanceServer.listen(serverName);
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &app, [&instanceServer]() {
        while (QLocalSocket *client = instanceServer.nextPendingConnection()) {
            client->deleteLater();
            emit Backend::instance()->raiseRequested();
        }
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("ClaudeMods.Hello", "Main");

    return app.exec();
}
