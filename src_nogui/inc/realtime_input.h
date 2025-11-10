
#ifndef DWRGFPSUNLOCKER_REALTIME_INPUT_H
#define DWRGFPSUNLOCKER_REALTIME_INPUT_H

#include "console_color_mgr.h"

#include <windows.h>
#include <iostream>
#include <vector>
#include <cctype>

class ConsoleInput {
private:
    HANDLE hIn;
    DWORD origMode;
    WORD activeColor;
    WORD defaultColor;
    ConsoleStyleManager& csm;
    bool activated;
    std::function<void()> whenactivated;
    bool exitFlag;
    std::atomic_bool forceExitFlag;
public:
    ConsoleInput(ConsoleStyleManager& csm, WORD defaultColor,  WORD activeColor, std::function<void()>&& whenactivated)
        :csm(csm), activeColor(activeColor), defaultColor(defaultColor), whenactivated(std::move(whenactivated))
        ,activated(false), exitFlag(false), forceExitFlag(false)
    {
        hIn = GetStdHandle(STD_INPUT_HANDLE);

        // 保存原模式
        DWORD mode;
        GetConsoleMode(hIn, &origMode);
        // 禁用行缓冲和回显
        SetConsoleMode(hIn, origMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
    }

    ~ConsoleInput() {
        // 恢复原模式
        SetConsoleMode(hIn, origMode);
    }

    int input(const std::string& prompt = "请输入数字后回车: ") {
        std::vector<char> buffer;
        std::cout << prompt;

        while (!exitFlag) {
            if (forceExitFlag)
            {
                return -1;
            }

            INPUT_RECORD records[10];
            DWORD read = 0;
            if (PeekConsoleInputA(hIn, records, 10, &read) && read > 0)
            {
                ReadConsoleInputA(hIn, records, read, &read);
                int coverage = 0;
                bool updated = false;
                for (auto i = 0; i< read; ++i)
                    if (records[i].EventType == KEY_EVENT && records[i].Event.KeyEvent.bKeyDown) {
                        auto ch = records[i].Event.KeyEvent.uChar.AsciiChar;

                        if (ch == 27) //esc直接退出
                        {
                            //todo: 按下ESC退出当前线程且主线程保持TYPING结束
                        }

                        if (std::isdigit(ch)) {
                            buffer.push_back(ch);
                            --coverage;
                            updated = true;
                        } else if (ch == '\r') {
                            std::cout << std::endl;
                            exitFlag = true;
                            break;
                        } else if (ch == '\b' && !buffer.empty()) {
                            buffer.pop_back();
                            ++coverage;
                            updated = true;
                        }
                    }
                if (updated)
                    refreshLine(prompt, buffer, coverage > 0 ? coverage : 0);
            }
            Sleep(16);
        }

        // 转换输入为整数
        int value = 0;
        for (char c : buffer)
            value = value * 10 + (c - '0');
        return value;
    }

    void force_exit()
    {
        forceExitFlag = true;
    }

private:
    void refreshLine(const std::string& prompt, const std::vector<char>& buffer, int coverextra = 0) {
        if (!activated)
        {
            activated = true;
            csm.setStyle(activeColor);
            whenactivated();
        }
        std::cout << '\r'<<prompt;
        csm.setStyle(FOREGROUND_GREEN);
        for (char c : buffer) std::cout << c;
        csm.resetStyle();
        if (coverextra) std::cout <<std::string(coverextra, ' '); // 清理被退格覆盖的字符?
        std::cout.flush();
    }
};


#endif //DWRGFPSUNLOCKER_REALTIME_INPUT_H