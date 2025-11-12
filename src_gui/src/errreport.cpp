//
// Created by Tofu on 2025/5/15.
//
#include "errreport.h"

#include <QMessageBox>

QMetaObject::Connection ErrorReporter::connection{};
ErrorReporter ErrorReporter::_instance;

ErrorReporter* ErrorReporter::instance() {
    // "C++11 后函数局部静态变量的初始化是线程安全的"，有线程锁，
    // 而且是延迟初始化的，因此实例在初次instance()时构造，
    // 而verbose的connect重入了instance，触发死锁. |1.5h ——25.10.29
    // static ErrorReporter _instance;
    return &_instance;
}

ErrorReporter::ErrorReporter(QObject *parent)
    : QObject(parent)
{
    // verbose();
    connection = QObject::connect(this/*直接用this*/, &ErrorReporter::report, &ErrorReporter::showError);
}

void ErrorReporter::showError(const ErrorInfo& einf)
{
    qDebug()<<"弹窗，内容："<<einf.msg;
    QMessageBox::critical(nullptr, einf.level,einf.msg);
    if (einf.level == ErrorReporter::严重)
    {
        qCritical()<<einf.msg;
        // emit ErrOccured();
    }
}

void ErrorReporter::receive(const ErrorInfo& einf) {
    instance()->report(einf);
}