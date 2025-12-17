#ifndef FPSDIALOG_H
#define FPSDIALOG_H

#include "ui_fpsdialog.h"

#include "fpssetter.h"

#include <QDialog>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class dwrgFpsSetter;
}
QT_END_NAMESPACE

class FpsDialog : public QDialog
{
    Q_OBJECT
    Ui::dwrgFpsSetter *ui;
    FpsSetter setter;

    QPalette frpalette;

    bool keepupdate;
    QTimer *frupdateremider; // 帧率tag刷新定时器
    QTimer *tmpreadtimer; // 临时帧率显示定时器

    QTimer *painlesssuicide; // bad窗自销定时器 /有必要吗？
    QMetaObject::Connection care2saveI;
    QMetaObject::Connection care2saveE;

    DWORD tied_gid;

    bool usedefault = false;
    bool bad = false;
public:
    FpsDialog(DWORD pid = NULL, QWidget *parent = nullptr);
    ~FpsDialog() override;

    operator bool() const
    {
        return !bad;
    }

    void set2tempread();

    void switch_default();

signals:
    void SetterClosed(FpsDialog*);
    void ErrOccured(FpsDialog*);


private slots:
    void on_applybutton_pressed();
    void on_curframerate_clicked();

private:
    //数据读写相关
    bool loadpreset();
    void savepreset()const;
    //帧率显示相关
    void dissmissFR();
    void updateFR();
    void whileKeepUpdateChange();
    void checkchangePalette();

    void applyFPS(int fps);

    void bebad();

    void on_fpscombox_touched()
    {
        disconnect(care2saveI), disconnect(care2saveE);
        delete painlesssuicide;
    }

    void closeEvent(QCloseEvent* event) override
    {
        emit SetterClosed(this);
    }

    friend bool gidequal(const FpsDialog&, DWORD tgid);
};

inline
bool gidequal(const FpsDialog& dialog, DWORD tgid)
{
    return dialog.setter.getGamePID() == tgid;
}
#endif // FPSDIALOG_H
