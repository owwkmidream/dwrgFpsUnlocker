#ifndef FPSDIALOG_H
#define FPSDIALOG_H

#include "env.h"

#include "ui_fpsdialog.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class dwrgFpsSetter;
}
QT_END_NAMESPACE

class FpsDialog : public QDialog
{
    Q_OBJECT
    Ui::dwrgFpsSetter *ui;
    FpsSetter* setter;

    QTimer *frupdatereminder; // 帧率tag刷新定时器
    bool keepupdate;
    QPalette frpalette;

    QTimer *tmpreadtimer; // 临时帧率显示定时器

    FpsDialog(QWidget *parent = nullptr);
public:
    static FpsDialog* create(DWORD pid = NULL);
    ~FpsDialog();

    void set2tempread();
    void dobuttonpress()
    {
        on_applybutton_pressed();
    }
signals:
    void SetterClosed(FpsDialog*);
    void ErrOccured(FpsDialog*);

private slots:
    void on_applybutton_pressed();
    // void on_applybutton_released() = delete;
    void on_curframerate_clicked();

private:
    bool checkload();
    void saveprofile()const;

    void tempreadreach();
    void checkchangePalette();
    void whileKeepUpdateChange();

    void updateFR();
    // QGraphicsDropShadowEffect* buttonShadow;
    void applyFPS(int fps);

    void closeEvent(QCloseEvent* event) override
    {
        emit SetterClosed(this);
    }
};
#endif // FPSDIALOG_H
