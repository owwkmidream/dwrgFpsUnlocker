//
// Created by Tofu on 25-6-23.
//

#include "unzip.h"

#include <windows.h>


int main(int argc, char *argv[])
{
    std::ios::sync_with_stdio(true);

    system("chcp 65001");

    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << "updater <压缩包路径> <项目目录> [等待结束的pid]" << std::endl;
        return 1;
    }

    std::filesystem::path zipPath = argv[1];
    std::filesystem::path outputDir = argv[2];

    //如果提供了pid则无限等待直至目标进程结束
    if (argc > 3)
    {
        int64_t waitpid = std::stoull(argv[3]);
        HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, waitpid);
        if (!hProcess)
        {
            std::cerr<<"出错：未能访问指定进程";
            return 1;
        }
        //Windows中对象本身就是可等待的
        WaitForSingleObject(hProcess, INFINITE);

        CloseHandle(hProcess);
    }

    if (!std::filesystem::exists(zipPath)) {
        std::cerr << "出错: 文件不存在: " << zipPath << std::endl;
        return 1;
    }

    //对于单可执行文件，直接复制过去就行
    if (zipPath.extension() == ".exe")
    {
        std::cout<<"拷贝文件："<<zipPath.filename()<<'\n';
        std::filesystem::copy_file(zipPath, outputDir);
        return 0;
    }
    //default:
    if (zipPath.extension() != ".zip")
    {
        std::cerr << "出错: 非zip文件: " << zipPath << std::endl;
        return 2;
    }

    auto unzipdir = zipPath.parent_path()/zipPath.stem();

    if( !std::filesystem::exists(unzipdir) || std::filesystem::is_directory(unzipdir) ) {
        std::cout<<"创建解压目录："<<unzipdir<<std::endl;
        std::filesystem::create_directory(unzipdir);
    }

    if(!unzipFile(zipPath, unzipdir))
        return 3;

    if(!std::filesystem::exists(outputDir)) {
        std::cerr<<"项目目录不存在？！"<<outputDir<<std::endl;
        std::filesystem::remove_all(unzipdir);
        return 1;
    }

    std::cout<<"拷贝覆盖...\n";
try {
    for (const auto &entry: std::filesystem::recursive_directory_iterator(unzipdir)) {
        auto relativePath = std::filesystem::relative(entry.path(), unzipdir);
        auto targetPath = outputDir / relativePath;

        if (entry.is_directory()) {
            std::filesystem::create_directories(targetPath);
            std::cout<<"拷贝目录："<<relativePath<<'\n';
        } else {
            if (std::filesystem::exists(targetPath)) {
                std::filesystem::remove(targetPath);
            }
            std::filesystem::copy_file(entry.path(), targetPath);
            std::cout<<"拷贝文件："<<relativePath<<'\n';
        }
    }

    std::cout<<"清理中...\n";
    // std::filesystem::remove_all(unzipdir);
} catch (const std::filesystem::filesystem_error &e) {
    std::cerr << "清理文件出错: " << e.what() << std::endl;
    return 1;
}

    return 0;
}