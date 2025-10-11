//
// Created by Tofu on 2025/6/26.
//

#ifndef DWRGFPSUNLOCKER_APPLIFEMGR_H
#define DWRGFPSUNLOCKER_APPLIFEMGR_H

#include <QObject>
#include <QApplication>

class UpdateChecker;
class FpsDialog;
class UpdateDialog;
class FpsSetter;

//todo 滥用信号槽？

class AppLifeManager:public QObject{
    Q_OBJECT
    QApplication& app;
    std::unique_ptr<UpdateChecker> udck;
    std::unique_ptr<UpdateDialog> inform;
    std::vector<FpsDialog*> windows;

    bool isVsetterX, isifmX;
public:
    AppLifeManager(QApplication &a, std::unique_ptr<UpdateDialog>&& win4show);

    template<class T>
    void trustee(std::unique_ptr<T>&& obj);
    template<class T>
    T& get();

private slots:
    void setterclosed(FpsDialog* that);
    void informclosed();
    void appquit();
private:
    void checkShouldQuit()
    {
        if(windows.empty() && isifmX)
        {
            appquit();
        }
    }
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
