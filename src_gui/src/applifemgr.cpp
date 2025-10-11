//
// Created by Tofu on 2025/6/26.
//

#include "applifemgr.h"
#include "updtchecker.h"
#include "fpsdialog.h"
//#include "updateinformer.h"
#include "fpssetter.h"

AppLifeManager::AppLifeManager(QApplication &a, std::unique_ptr<UpdateDialog>&& win4show) :
app(a), inform(std::move(win4show)), isVsetterX(false), isifmX(false)
{
    connect(udck.get(), &UpdateChecker::noUpdateAvailable, this, &AppLifeManager::informclosed);
}

//对一个尚未show的窗口close不会触发closeEvent。。。

void AppLifeManager::setterclosed(FpsDialog* that) {
    auto it = std::find(windows.begin(), windows.end(), that);
    if (it == windows.end())
    {
        ErrorReporter::receive(ErrorReporter::严重, "销毁一个未记录的窗口");
        qCritical()<<"销毁一个未记录的窗口";
        return;
    }
    windows.erase(it);
    delete that;
}

void AppLifeManager::informclosed() {
    // udck.reset(); //nononono you are the last 救命稻草
    isifmX = true;
}

void AppLifeManager::appquit() {
    app.quit();
}

template<>
void AppLifeManager::trustee<FpsDialog>(std::unique_ptr<FpsDialog>&& dialog)
{
    auto w_ = dialog.release();
    windows.push_back(w_);
    QObject::connect(w_, &FpsDialog::SetterClosed, this, &AppLifeManager::setterclosed);
    QObject::connect(w_, &FpsDialog::ErrOccured, this, &AppLifeManager::setterclosed);
}

template<>
void AppLifeManager::trustee(std::unique_ptr<UpdateDialog>&& ifm)
{
    //右值是对调用方的约束，在函数体内是新的左值，所以移动赋值的话还是需要移动语义
    this->inform = std::move(ifm);
    QObject::connect(this->inform.get(), &UpdateDialog::InformerClose, this, &AppLifeManager::informclosed);
}
template<>
void AppLifeManager::trustee(std::unique_ptr<UpdateChecker>&& chk)
{
    this->udck = std::move(chk);
}

