
#ifndef LOG_H
#define LOG_H

#include <QFile>
#include <QDir>
#include <QEvent>
#include <QMetaEnum>
#include <QTimer>
#include <QMutex>


//@ref DevDoc/logging.cov.md
inline
void customLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
#ifndef USE_LOG
    Q_UNUSED(type)
    Q_UNUSED(context)
    Q_UNUSED(msg)
    return;
#endif
    static QMutex mutex;
    QMutexLocker lock(&mutex);//自动阻塞-加锁-释放类

    static QFile logFile;
    static bool inited = false;

    if (!inited) {
        QString logDir = "logs";
        QDir().mkpath(logDir);

        QString logName = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss.log");
        logFile.setFileName(QString("%1/%2").arg(logDir, logName));

        logFile.open(QIODevice::Append | QIODevice::Text);
        inited = true;
    }

#ifndef _DEBUG
    if (type == QtDebugMsg)
    {
        return;
    }
#endif

    // 格式化日志信息
    QString logType;
    QTextStream out(&logFile);
    switch (type) {
    case QtDebugMsg:
        logType = "[DEBUG]";
        break;
    case QtInfoMsg:
        logType = "[INFO]";
        break;
    case QtWarningMsg:
        logType = "[WARNING]";
        break;
    case QtCriticalMsg:
        logType = "[CRITICAL]";
        break;
    case QtFatalMsg:
        logType = "[FATAL]";
        break;
    }
    auto time = QTime::currentTime();
    out << logType << QString(": %1:%2: ").arg(time.minute()).arg(time.second())
        << msg
#ifdef _DEBUG
        << ", Function:" << context.function << ")"
#endif
        << '\n';
}

#ifdef _DEBUG
class EventSpy : public QObject
{
    bool inited = false;
    QFile logFile;
public:
    EventSpy(QObject *parent = nullptr) : QObject(parent) {}
    ~EventSpy() override {logFile.close();}
protected:
    bool eventFilter(QObject *obj, QEvent *evt)
    {

        if (!inited) {
            QString logDir = "logs";
            QDir().mkpath(logDir);

            QString logName = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
        static  int spy_id = 0;
            logFile.setFileName(QString("%1/%2.%3.qevts").arg(logDir, logName, QString::number(spy_id++)));

            logFile.open(QIODevice::WriteOnly|QIODevice::Text);
            inited = true;
        }

        static const QMetaEnum metaEnum = QMetaEnum::fromType<QEvent::Type>();
        const char *typeName = metaEnum.valueToKey(evt->type());

        QString objInfo;
        if (obj->objectName().isEmpty())
            objInfo = QString("%1(%2)").arg(obj->metaObject()->className())
                                       .arg((quintptr)obj, 0, 16);
        else
            objInfo = QString("%1(%2)").arg(obj->metaObject()->className())
                                       .arg(obj->objectName());

        QTextStream out(&logFile);
        out
            << QString("[Event] %1 on %2  (type=%3)\n")
                  .arg(typeName ? typeName : "Unknown", -25)
                  .arg(objInfo, -30)
                  .arg(evt->type());

        return false;// QObject::eventFilter(obj, evt); ← ai说这是个陷阱，这就是return false
    }
};
#else
#define spy nullptr
// app.installEventFilter(&spy);
#endif

#endif