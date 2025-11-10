//
// Created by Tofu on 25-6-19.
//

#ifndef DWRGFPSUNLOCKER_CONSOLE_COLOR_MGR_H
#define DWRGFPSUNLOCKER_CONSOLE_COLOR_MGR_H

#include <windows.h>

class ConsoleStyleManager
{
private:
    HANDLE hConsole;// 控制台句柄
    WORD defaultAttributes; // 控制台的默认输出风格

public:
    WORD queryDefault()
    {
        return defaultAttributes;
    }
    ConsoleStyleManager()
    {
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        // 保存当前控制台的输出风格
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        defaultAttributes = csbi.wAttributes;
    }

    // 设置控制台输出颜色
    ConsoleStyleManager& setStyle(WORD attributes) {
        SetConsoleTextAttribute(hConsole, attributes ? attributes : defaultAttributes);
        return *this;
    }
    ConsoleStyleManager& operator()(WORD attributes)
    {
        return setStyle(attributes);
    }

    // 恢复控制台默认颜色
    ConsoleStyleManager& resetStyle() {
        SetConsoleTextAttribute(hConsole, defaultAttributes);
        return *this;
    }

    // 析构函数，自动恢复控制台默认颜色
    ~ConsoleStyleManager() {
        resetStyle();
    }
    friend std::ostream& operator<<(std::ostream& os, const ConsoleStyleManager& consoleStyleManager)
    {
        return os;
    }
};


#endif //DWRGFPSUNLOCKER_CONSOLE_COLOR_MGR_H
