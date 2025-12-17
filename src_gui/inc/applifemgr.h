//
// Created by Tofu on 2025/6/26.
//

#ifndef DWRGFPSUNLOCKER_APPLIFEMGR_H
#define DWRGFPSUNLOCKER_APPLIFEMGR_H

#include <QObject>
#include <QApplication>

#include "intsafe.h"

class UpdateChecker;
class FpsDialog;
class UpdateDialog;
class FpsSetter;


class AppLifeManager:public QObject{
    Q_OBJECT
    QApplication& app;
    std::unique_ptr<UpdateChecker> udck;
    std::unique_ptr<UpdateDialog> inform;
    std::vector<FpsDialog*> windows;

public:
    AppLifeManager(QApplication &a);
    ~AppLifeManager() override;

    template<class T>
    void trustee(std::unique_ptr<T>&& obj);
    template<class T>
    void trustee(T* obj);
    template<class T, typename... Args>
    requires std::constructible_from<T, Args...>
    T& delever(Args... args);

    template<class T>
    T& get();

    void foreachwin(const std::function<void(FpsDialog&)>& execution)
    {
        for (FpsDialog* window : windows)
            execution(*window);
    }

private slots:
    void setterclosed(FpsDialog* that);

    void closeupdate();
    void appquit();
};

template<> inline
UpdateChecker& AppLifeManager::get()
{
    return *udck;
}

template<> inline
UpdateDialog& AppLifeManager::get()
{
    return *inform;
}


#endif //DWRGFPSUNLOCKER_APPLIFEMGR_H
