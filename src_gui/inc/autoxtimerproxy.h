//
// Created by Tofu on 2025/5/15.
//

#ifndef AUTOXTIMERPROXY_H
#define AUTOXTIMERPROXY_H

#include "fpssetter.h"
#include "errreport.h"

#include <QObject>
#include <QTimer>

//因为FpsSetter不能依赖Qt（信号槽机制需要QObject），因此借助这个代理类间接调用closeHandle
class autoxtimerproxy: public QObject
{
    Q_OBJECT
    QTimer timer;
    FpsSetter* belongsto;
public:
    autoxtimerproxy(FpsSetter* master) : belongsto(master)
    {
        timer.setSingleShot(true);
        timer.setInterval(15000);
        connect(&timer, &QTimer::timeout, this, &autoxtimerproxy::xhandlerauto);
    }

    autoxtimerproxy(autoxtimerproxy&) = delete;
    autoxtimerproxy& operator=(autoxtimerproxy&) = delete;

    autoxtimerproxy(autoxtimerproxy&&) = delete;
    autoxtimerproxy& operator=(autoxtimerproxy&&) = delete;

    ~autoxtimerproxy() override
    {
        timer.stop();
    }
    bool isRunning() const
    {
        return timer.isActive();
    }
    void stop()
    {
        timer.stop();
    }
    void start()
    {
        timer.start();
    }
private slots:
    void xhandlerauto()
    {
        belongsto->closeHandle();
#ifdef _DEBUG
        ErrorReporter::receive("提醒", "句柄关闭定时任务执行。");
#endif
    }
};

#endif //AUTOXTIMERPROXY_H
