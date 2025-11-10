#ifndef DUPLICATE_H
#define DUPLICATE_H

#include <QLocalServer>
#include <QLocalSocket>

//初次创建时找到唯一实例，并通信创建一次窗口
class Copy: public QObject
{
    Q_OBJECT
    QLocalServer* m_server;
    bool isdup;
public:
    Copy(const QString& servername)
    {
        QLocalSocket socket;
        socket.connectToServer(servername);

        if (isdup = socket.waitForConnected(100)) {
            qDebug()<<"连接到单例";
            socket.write("prpr");
            socket.flush();
            socket.waitForBytesWritten(100);
            socket.disconnectFromServer();
            return;
        }

        // 不存在 -> 自己创建
        // QFile::remove(servername); // 清理残留文件
        m_server = new QLocalServer(this);
        if (!m_server->listen(servername)) {
            qWarning()<<"监听套接字失败："<<m_server->errorString();
            return;
        }
        connect(m_server, &QLocalServer::newConnection, this, &Copy::listen);

        QMetaObject::invokeMethod(this, "SilentLaunch", Qt::QueuedConnection);
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

#endif