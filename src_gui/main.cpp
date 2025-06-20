#include "fpsdialog.h"
#include "ui_fpsdialog.h"

#include "env.h"
#include "errreport.h"

#include <QApplication>
#include <QTranslator>
#include <QFile>

bool initial()
{
    HWND targetWindow = FindWindowW(nullptr, L"第五人格");
    if (!targetWindow)
    {
        ErrorReporter::instance()->receive({"只是一昧报错","未找到第五人格窗口"});
        return false;
    }

    DWORD pid;
    GetWindowThreadProcessId(targetWindow, &pid);
    std::cout << "找到窗口，进程ID: " << pid << std::endl;

    HMODULE dll = LoadLibraryW(L"address.dll");
    if(!dll)
        ErrorReporter::instance()->receive(ErrorReporter::严重, "无法加载address.dll");
    using GetterRaw = FpsSetter*(*)(DWORD);
    auto raw_fn = (GetterRaw)GetProcAddress(dll, "CreateInstance");
    if(!raw_fn)
        ErrorReporter::instance()->receive(ErrorReporter::严重, "无法于address.dll找到目标函数");


    setter = std::unique_ptr<FpsSetter>(raw_fn(pid));
    if (*setter)
        return false;

    return true;
}

bool checkload(Ui::dwrgFpsSetter* ui)
{
    if(hipp->exists())
    {
        if(hipp->open(QIODevice::ReadOnly))
        {
            QDataStream ds(hipp.get());

            int fps;
            ds>>fps;

            bool uselast;
            ds>>uselast;
            if(uselast)
            {
                ui->fpscombox->setCurrentText(QString::number(fps));
                ui->autoappradio->setChecked(true);
                dl->dobuttonpress();
            }
        }
        else
        {
            ErrorReporter::instance()->receive({"错误", "无法访问文件 ./hipp "});
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;

    Dialog w;
    w.setWindowTitle("第五帧率解锁");
    w.setFixedSize(w.width(), w.height());

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    if (!initial())
    {
        QApplication::exit(1);
        exit(1);
    }

    if (!checkload(dl->getui()))
    {
        QApplication::exit(2);
        exit(2);
    }

    w.show();

    return a.exec();
}
