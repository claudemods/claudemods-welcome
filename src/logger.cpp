#include "logger.h"

#include "externals.h"
#include "utils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdio>

namespace logger {

namespace {

QFile *g_file = nullptr;
QMutex g_mutex;

void handler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const char *level = "DEBUG";
    switch (type) {
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    default: break;
    }
    const QString line =
        QStringLiteral("%1 %2 %3\n").arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), QLatin1String(level), msg);

    QMutexLocker lock(&g_mutex);
    std::fputs(line.toLocal8Bit().constData(), stderr);
    if (g_file && g_file->isOpen()) {
        g_file->write(line.toUtf8());
        g_file->flush();
    }
}

} // namespace

void setup()
{
    const QString dir = utils::fixPath(QString::fromLatin1(ext::kLogDir));
    QDir().mkpath(dir);
    g_file = new QFile(dir + QLatin1Char('/') + QString::fromLatin1(ext::kLogFile));
    g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(handler);
}

} // namespace logger
