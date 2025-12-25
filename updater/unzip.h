//
// Created by Tofu on 2025/12/20.
//

#ifndef DWRGFPSUNLOCKER_UNZIP_H
#define DWRGFPSUNLOCKER_UNZIP_H

#include <ctime>
#include <cstdint>

#include <mz_zip.h>
#include <mz.h>
#include <mz_strm_os.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <array>

#define READ_SIZE 8192

#include <filesystem>
#include <string>
#include <system_error>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_strm_os.h>

bool unzipFile(const std::filesystem::path &zipPath, const std::filesystem::path &outputDir) {
    int32_t err = MZ_OK;

    // 确保输出根目录存在
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
        std::cerr << "无法创建输出目录: " << ec.message() << std::endl;
        return false;
    }

    // 创建文件流
    auto zip_stream = mz_stream_os_create();
    // 打开 ZIP 文件
    err = mz_stream_os_open(zip_stream, zipPath.string().c_str(), MZ_OPEN_MODE_READ);
    if (err != MZ_OK) {
        std::cerr << "无法打开ZIP文件: " << err << std::endl;
        mz_stream_os_delete(&zip_stream);
        return false;
    }

    // 创建 ZIP 阅读器
    auto zip_reader = mz_zip_create();
    // 将流附加到 ZIP 阅读器
    err = mz_zip_open(zip_reader, zip_stream, MZ_OPEN_MODE_READ);
    if (err != MZ_OK) {
        std::cerr << "无法打开ZIP: " << err << std::endl;
        mz_zip_delete(&zip_reader);
        mz_stream_os_close(zip_stream);
        mz_stream_os_delete(&zip_stream);
        return false;
    }

    // 遍历所有条目
    err = mz_zip_goto_first_entry(zip_reader);
    if (err == MZ_END_OF_LIST) {
        // ZIP 文件为空 - 正常情况
        std::cout << "ZIP文件为空" << std::endl;
        mz_zip_close(zip_reader);
        mz_zip_delete(&zip_reader);
        mz_stream_os_close(zip_stream);
        mz_stream_os_delete(&zip_stream);
        return true;
    }

    if (err != MZ_OK) {
        std::cerr << "无法定位到第一个条目: " << err << std::endl;
        mz_zip_close(zip_reader);
        mz_zip_delete(&zip_reader);
        mz_stream_os_close(zip_stream);
        mz_stream_os_delete(&zip_stream);
        return false;
    }

    // 遍历并处理所有条目
    do {
        mz_zip_file *file_info = nullptr;

        // 获取当前条目信息
        err = mz_zip_entry_get_info(zip_reader, &file_info);
        if (err != MZ_OK) {
            std::cerr << "无法获取条目信息: " << err << std::endl;
            break;
        }

        std::string entry_name = file_info->filename;

        // 检查是否是目录
        bool is_directory = false;

        if (mz_zip_entry_is_dir(zip_reader) == MZ_OK) {
            is_directory = true;
        }

        // 构建完整输出路径
        std::filesystem::path output_path = outputDir / entry_name;

        if (is_directory) {
            // 处理目录项
            std::cout << "创建目录: " << output_path << std::endl;

            // 确保目录路径标准化（移除结尾的斜杠）
            std::string dir_path = output_path.string();
            if (!dir_path.empty() && (dir_path.back() == '/' || dir_path.back() == '\\')) {
                dir_path.pop_back();
            }

            // 创建目录
            if (!std::filesystem::exists(dir_path)) {
                if (!std::filesystem::create_directories(dir_path, ec)) {
                    std::cerr << "无法创建目录 " << dir_path << ": " << ec.message() << std::endl;
                    // 不一定要失败，继续处理其他文件
                }
            }

            // 目录没有数据需要读取，直接继续下一个
            continue;
        } else {
            // 处理文件项
            std::cout << "解压文件: " << output_path << std::endl;

            // 打开输出文件
            void *output_stream = mz_stream_os_create();
            err = mz_stream_os_open(output_stream, output_path.string().c_str(),
                                   MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE);
            if (err != MZ_OK) {
                std::cerr << "无法打开输出文件 " << output_path << ": " << err << std::endl;
                mz_stream_os_delete(&output_stream);
                // 继续处理其他文件
                continue;
            }

            // 准备读取条目数据
            err = mz_zip_entry_read_open(zip_reader, 0, nullptr);
            if (err != MZ_OK) {
                std::cerr << "无法读取条目 " << entry_name << ": " << err << std::endl;
                if (err == MZ_SUPPORT_ERROR)
                {

                }
                mz_stream_os_close(output_stream);
                mz_stream_os_delete(&output_stream);
                continue;
            }

            // 读取并写入文件数据
            uint8_t buffer[8192];
            int32_t read_bytes = 0;
            bool write_error = false;

            do {
                read_bytes = mz_zip_entry_read(zip_reader, buffer, sizeof(buffer));
                if (read_bytes > 0) {
                    int32_t written_bytes = mz_stream_os_write(output_stream, buffer, read_bytes);
                    if (written_bytes != read_bytes) {
                        std::cerr << "写入文件失败: " << entry_name << std::endl;
                        write_error = true;
                        break;
                    }
                }
            } while (read_bytes > 0);

            // 关闭条目和输出流
            mz_zip_entry_close(zip_reader);
            mz_stream_os_close(output_stream);
            mz_stream_os_delete(&output_stream);

            if (write_error) {
                // 删除不完整的文件
                std::filesystem::remove(output_path, ec);
            }
        }

    } while ((err = mz_zip_goto_next_entry(zip_reader)) == MZ_OK);

    // 清理资源
    mz_zip_close(zip_reader);
    mz_zip_delete(&zip_reader);
    mz_stream_os_close(zip_stream);
    mz_stream_os_delete(&zip_stream);

    // 检查循环结束的原因
    if (err != MZ_END_OF_LIST && err != MZ_OK) {
        std::cerr << "解压过程中发生错误: " << err << std::endl;
        return false;
    }

    std::cout << "解压完成" << std::endl;
    return true;
}

#endif //DWRGFPSUNLOCKER_UNZIP_H