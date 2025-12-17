#ifndef DUPLICATE_H
#define DUPLICATE_H

#include <QLocalServer>
#include <QLocalSocket>

#include <QWidget>

//初次创建时找到唯一实例，并通信创建一次窗口
class Copy: public QObject
{
    Q_OBJECT
    QLocalServer* m_server;
    bool isdup;
    QString svname;

public:
    Copy( QString servername):svname(std::move(servername))
    {
        QLocalSocket socket;
        socket.connectToServer(svname);

        if ((isdup = socket.waitForConnected(100))) {
            qDebug()<<"连接到单例";
            socket.write("prpr");
            socket.flush();
            socket.waitForBytesWritten(100);
            socket.disconnectFromServer();
            return;
        }

        // 不存在 -> 自己创建
        m_server = new QLocalServer(this);
        if (!m_server->listen(svname)) {
            qWarning()<<"监听套接字失败："<<m_server->errorString();
            return;
        }
        connect(m_server, &QLocalServer::newConnection, this, &Copy::listen);

        QMetaObject::invokeMethod(this, "SilentLaunch", Qt::QueuedConnection);
    }
    ~Copy()
    {
        m_server->removeServer(svname);
    }

    operator bool() const
    {
        return isdup;
    }

    signals:
    void NewLaunch();
    void SilentLaunch();

private slots:
    void listen()
    {
        auto *client = m_server->nextPendingConnection();
        client->deleteLater();
        emit NewLaunch();
    }
};

class HiddenWin:public QWidget
{
public:
    HiddenWin(QWidget *parent = nullptr):QWidget(parent)
    {
        setAttribute(Qt::WA_DontShowOnScreen);
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
        show();
    }
};

#endif