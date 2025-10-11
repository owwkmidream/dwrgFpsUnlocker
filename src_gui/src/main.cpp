#include "macroes.h"

#include "applifemgr.h"
#include "errreport.h"
#include "fpsdialog.h"
#include "updtdialog.h"
#include "env.h"
#include "winapiutil.h"
#include "gamelistener.h"

#include <QDir>
#include <QApplication>
#include <QSystemTrayIcon>
#include <QFile>
#include <QMenu>

#include <memory>

//@ref DevDoc/logging.cov.md
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

    out << logType << ": "
        << msg
#ifdef _DEBUG
        << ", Function:" << context.function << ")"
#endif
        << '\n';
}

int main_update(int, char **);
int main_setter(int, char **);

#ifndef GUI_BUILD_SINGLE
#define main_update sizeof
#endif
int main(int argc, char *argv[])
{
    switch (argc)
    {
        case 1 : case 2: return main_setter(argc, argv);
        default: return main_update(argc, argv);
    }
}
#undef main_update

int main_setter(int argc, char *argv[])
{
    qInstallMessageHandler(customLogHandler);

    qInfo()<<"====环境信息========================";
    qInfo()<< "版本: "<<VERSION_STRING;
    qInfo()<< PrintProcessGroups();

    QApplication app(argc, argv);
    QApplication::setApplicationVersion(VERSION_STRING);
    QApplication::setQuitOnLastWindowClosed(false);//手动管理

    auto ifm{std::make_unique<UpdateDialog>()};
    auto uc(std::make_unique<UpdateChecker>(*ifm));
    ifm->setRelativeData(*uc);//todo: 担心引用失效

    AppLifeManager lifemgr(app, std::move(ifm));

    lifemgr.trustee(std::move(uc));
    //此后的ifm 和 uc 都默认失效了

    lifemgr.get<UpdateChecker>().checkUpdate();

    ProgStartListener listener;
    listener.setProcessName("dwrg.exe");
    listener.setPollInterval(3);
    //lifemgr是主线程局部变量所以它销毁了事件循环肯定也结束了所以引用它没问题。
    QObject::connect(&listener, &ProgStartListener::processStarted,[&lifemgr](quint32 pid)
    {
        qInfo()<<"检测到游戏启动，pid:"<<pid;
        QTimer::singleShot(3000, &lifemgr, [&lifemgr,pid]()
        {
            std::unique_ptr<FpsDialog> w_{FpsDialog::create(pid)};
            w_->show();w_->raise();w_->activateWindow();
            lifemgr.trustee(std::move(w_));
        });
    });
    listener.start();

    auto tray = QSystemTrayIcon();
    QIcon ico(":/纯彩mini.ico");//todo: 字符编码问题
    if (ico.isNull())
    {
        ErrorReporter::receive(ErrorReporter::警告, "图标资源错误");
        qWarning()<<"图标资源错误";
    }
    else
    {
        tray.setIcon(ico);
        tray.show();
        QMenu* trayMenu = new QMenu(&lifemgr.get<UpdateDialog>());
        trayMenu->addAction("退出", &app, &QApplication::quit);
        tray.setContextMenu(trayMenu);
    }

    // setter的初始化报错时lifemgr需要已经注册信号槽，因此setter必须在lifemgr之后初始化
    qInfo()<<"====运行========================";
    return app.exec();
}

[[maybe_unused]]
int main_update(int argc, char *argv[])
{
    std::filesystem::path newExePath = argv[1];
    std::filesystem::path curDir = argv[2];
    if (argc > 3)
    {
        int64_t waitpid = std::stoull(argv[3]);
        HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, waitpid);
        if (!hProcess)
        {
            std::cerr<<"出错：未能访问指定进程";
            return 1;
        }
        //Windows中对象本身就是可等待的
        WaitForSingleObject(hProcess, INFINITE);

        CloseHandle(hProcess);
    }
    auto to = curDir/newExePath.filename();
    try
    {
        std::filesystem::copy(newExePath, to, std::filesystem::copy_options::overwrite_existing);
    }catch (std::exception&e)
    {
        qCritical()<<e.what();
        return -1;
    }
    return 0;
}

//todo: 响应ctrlC
//todo: 添加进程结束监听
//todo: 失效setter销毁