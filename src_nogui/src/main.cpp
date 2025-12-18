//
// Created by Tofu on 2025/5/15.
//

#include "storage.h"

#include "fpssetter.h"
#include "realtime_input.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>


int main()
{
    system("chcp 65001");
    std::cout<<std::setiosflags(std::ios::fixed)<<std::setprecision(2);

    auto setter = FpsSetter::create();

    ConsoleStyleManager csm;

    auto repeat_fps = [&](int times = 5)
    {
        csm.setStyle(FOREGROUND_GREEN);
        while (--times)
        {
            std::cout<<setter.getFps();
            Sleep(1000);
            std::cout<<'\r';
        }
        std::cout<<setter.getFps()<<'\r';
    };

    int fps = 60;
    bool havensettle = false;
    using Hipp = Storage<hipp,"hipp">;
    if (Hipp::exist())
    {
        if (!Hipp::available())
        {
            std::cerr << "打不开文件 hipp..\n";
            Sleep(1500);
            exit(-5);
        }
        fps = Hipp::load<&hipp::fps>();
        if (fps <= 0) fps = 60;

        setter.setFps(fps);
        havensettle = true;
        std::cout<<"自动设置上次的帧率值: "<<csm(FOREGROUND_GREEN)<<fps<<'\n';
        repeat_fps();
        csm.resetStyle();
    }

    enum INPUT_STATE{YET, TYPING, RETURN} input_state = YET;

    auto ci = new ConsoleInput(csm, FOREGROUND_INTENSITY, NULL,
            [&input_state] {input_state = TYPING;});

    std::thread inputThread([&]() {
        if (havensettle)
            csm.setStyle(FOREGROUND_INTENSITY);

        fps = ci->input("输入期望帧率的正整数值并回车：");

        delete ci;
        ci = nullptr;

        //只有typing才表示输入过东西
        if (input_state == TYPING)
            input_state = RETURN;

        //0表示输入过删了，就算没输
        if (fps == 0)
            input_state = YET;

        csm.resetStyle();
    });

    int timeout = 5*2; // 超时时间（秒）
    for (int i = 0; input_state == TYPING || i< timeout; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds (500));
    }
    //question: 但是我们会在YET的时候超时quit，这种情况下是要退堂的
    //          我们有办法区分timeout是yet还是typing呢？
    if (input_state != RETURN)//出来可能是因为超时
    {
        ci->force_exit();
        if (inputThread.joinable())
            inputThread.join();
    }

    if (input_state == YET)
    {
        csm.resetStyle();
        std::cout << "\n(－_－)"<<std::flush;
        if (inputThread.joinable())
        {
            inputThread.join(); // 分离子线程，确保程序退出
        }
        Sleep(1500);
        std::cout<<"，退堂！"<<std::flush;
        Sleep(500);
    }
    else if (input_state == RETURN)
    {
        setter.setFps(fps);
        repeat_fps();

        Hipp::save<&hipp::fps>(fps);
        if (Hipp::available())
            Hipp::dosave();

        std::cout<<'\n';
        inputThread.join();
        Sleep(2000);
    }

    return 0;
}