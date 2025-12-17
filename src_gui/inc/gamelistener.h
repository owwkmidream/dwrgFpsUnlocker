//
// Created by Tofu on 2025/10/11.
//

#ifndef DWRGFPSUNLOCKER_GAMELISTENER_H
#define DWRGFPSUNLOCKER_GAMELISTENER_H


#include <QThread>
#include <QString>
#include <QAtomicInt>

#include <intsafe.h>

//todo: 添加进程结束监听
class ProgStartListener : public QObject
{
    Q_OBJECT
public:
    explicit ProgStartListener(QObject* parent = nullptr);
    ~ProgStartListener() override;

    Q_INVOKABLE void start(const QString& exeName, int interval_sec);
    Q_INVOKABLE void stop();

    signals:
    // 当检测到目标进程创建时发射（在 monitor 线程中发出，但通过 queued connection 回到主线程）
    void processStarted(DWORD pid);

    // 状态/错误信息（可选）
    void errorOccured(const QString& message);
    void started();
    void stopped();

private:
    //不知道干嘛的
    // QThread m_workerThread;
    //是否在运行
    QAtomicInt m_running;
    //创建过的线程
    QList<QThread*> m_threads;
};


#endif //DWRGFPSUNLOCKER_GAMELISTENER_H