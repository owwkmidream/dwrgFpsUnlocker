//
// Created by Tofu on 2025/10/11.
//

#ifndef DWRGFPSUNLOCKER_GAMELISTENER_H
#define DWRGFPSUNLOCKER_GAMELISTENER_H


#include <QObject>
#include <QThread>
#include <QString>
#include <QAtomicInt>

class ProgStartListener : public QObject
{
    Q_OBJECT
public:
    explicit ProgStartListener(QObject* parent = nullptr);
    ~ProgStartListener();

    void setProcessName(const QString& exeName);

    // 轮询间隔（秒），用于 WMI WITHIN 子句。默认 1 秒。
    void setPollInterval(int seconds);

    // 开始 / 停止监测（非阻塞）
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    signals:
        // 当检测到目标进程创建时发射（在 monitor 线程中发出，但通过 queued connection 回到主线程）
    void processStarted(quint32 pid);

    // 状态/错误信息（可选）
    void errorOccured(const QString& message);
    void started();
    void stopped();

private:
    QString m_exeName;
    int m_intervalSeconds;

    QThread m_workerThread;
    QAtomicInt m_running;

    QList<QThread*> m_threads;
};


#endif //DWRGFPSUNLOCKER_GAMELISTENER_H