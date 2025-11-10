#pragma once

#include "macroes.h"

#include <ylt/reflection/member_value.hpp>
#include <ylt/reflection/member_names.hpp>

#include <QDebug>

#include <fstream>
#include <algorithm>
#include <shared_mutex>
#include <functional>

/** 存储布局
 *  需要是平凡的；建议提供默认值。
 */
struct hipp
{
    int fps = 60;
    bool checked = false;
};

//能作为字符串模板常量的hack
template <size_t N>
struct fixed_string {
    char data[N];
    constexpr fixed_string(const char (&str)[N]) {
        std::copy_n(str, N, data);
    }
    operator std::string() const
    {
        //指针+长度的构造
        std::string ret{data, N-1};
        return ret;
    }
    constexpr std::string_view view() const noexcept { return {data, N - 1}; }
    constexpr fixed_string filenamify()const
    {
        fixed_string ret = *this;
        std::replace(ret.data, ret.data+N, ':', '-');
        return ret;
    }
};

template<typename Layout, fixed_string storagename>
class Storage
{
public:
        //确保存储可用
    operator bool() const;
static bool available();

        //检查是否存有记录
static bool exist();

   /** 存
    * 请确保存储可用(bool/available)*/
template<auto Member, typename T>
static
    void save(T&& value);

   /** 读
    * 请确保存储可用(bool/available)*/
template<auto Member>
static
    auto load();

    //清空记录
static
    void clear();

   /** 写入存储
    * 请先确保存储可用(bool/available) */
static
    void dosave();

protected:
    //编译期生成的成员名称数组 和 成员偏移数组
    inline static const auto keys = ylt::reflection::get_member_names<Layout>();
    //缓存 和 字段是否为空的记录
static inline
    std::array<bool, ylt::reflection::members_count<Layout>()> hascached{};
static inline
    Layout cache{};
#ifndef BUILD_SINGLE
static inline
    std::fstream _file ;
    enum{BAD, YEW=0, HAVE};
static inline
    int8_t held = YEW;
#else
    static inline std::wstring storagename_w{U8_2_U16(storagename)};

    static bool                 cred_write (const std::string_view& key, uint8_t* data, size_t size);
    static std::vector<uint8_t> cred_read  (const std::string_view& key);
    static bool                 cred_delete(const std::string_view& key);
#endif
};

#ifdef BUILD_SINGLE

#include "winapiutil.h"
#include "wincred.h"

template <typename Layout, fixed_string storagename>
bool Storage<Layout, storagename>::cred_write(const std::string_view& key, BYTE* data, size_t size)
{
    std::wstring targetname = storagename_w+U8_2_U16(key);

    CREDENTIALW cred = {};

    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<wchar_t*>(targetname.c_str());
    cred.CredentialBlobSize = size;
    cred.CredentialBlob = data;
    cred.Persist = CRED_PERSIST_ENTERPRISE;  // 漫游凭据

    return CredWriteW(&cred, 0);
}

template <typename Layout, fixed_string storagename>
std::vector<uint8_t> Storage<Layout, storagename>::cred_read(const std::string_view& key)
{
    PCREDENTIALW pCredential;
    std::wstring targetname = storagename_w+U8_2_U16(key);
    std::vector<BYTE> result;

    if (CredReadW(targetname.c_str(), CRED_TYPE_GENERIC, 0, &pCredential)) {
        if (pCredential->CredentialBlobSize > 0) {
            result.assign(pCredential->CredentialBlob,
                          pCredential->CredentialBlob + pCredential->CredentialBlobSize);
        }
        CredFree(pCredential);
    }

    return result;
}

template <typename Layout, fixed_string storagename>
bool Storage<Layout, storagename>::cred_delete(const std::string_view& key)
{
    std::wstring targetname = storagename_w+U8_2_U16(key);
    return CredDeleteW(targetname.c_str(), CRED_TYPE_GENERIC, 0);
}
template<typename Layout, fixed_string storagename>
    Storage<Layout, storagename>::operator bool()const
    {return true;}

template <typename Layout, fixed_string storagename>
    bool Storage<Layout, storagename>::available()
    {return true;}

template<class Layout, fixed_string storagename>
    bool Storage<Layout, storagename>::exist()
    {
        qDebug()<<"检查是否有过记录";
        static std::optional<bool> _exist = std::nullopt;
        if (_exist)
            return *_exist;

        bool vkeyexist = true;
        ylt::reflection::for_each(cache, [&vkeyexist](auto& member, auto key, auto index)
        {
            if (vkeyexist == false)
                return;

            auto result = cred_read(key);
            if (result.empty())
                vkeyexist = false;
            else //顺便把load给做了
                memcpy(&member, result.data(), sizeof(member));
            hascached[index] = true;
        });
        qDebug()<<(vkeyexist?"存在":"无")<<"记录";
        return *(_exist = vkeyexist);
    }

template<class Layout, fixed_string storagename>
    void Storage<Layout, storagename>::clear()
    {
        qInfo()<<"清理所有记录..";
        for (auto key : keys)
        {
            cred_delete(key);
        }
        cache = Layout{};
        hascached = {};
    }

template <typename Layout, fixed_string storagename>
    void Storage<Layout, storagename>::dosave()
    {
        ylt::reflection::for_each(cache, [&](auto& member, auto key, auto index)
        {
            cred_write(key, (BYTE*)&member, sizeof(member));

            // const QByteArray data( reinterpret_cast<char*>(&member) + ylt::reflection::member_offsets<Layout>[index], sizeof(member));
            //                                                 ↑ 这个就是member我当成cache还加偏移，真是蠢死了 --25.11.9*/
        });
        qDebug()<<"记录落盘";
    }

template<class Layout, fixed_string storagename>
    template<auto Member>
    auto Storage<Layout, storagename>::load()
    {
        const auto &index = ylt::reflection::index_of<Member>();
        const auto& key = keys[ylt::reflection::index_of<Member>()];

        qDebug()<<"准备读取"<<key;
        if (hascached[index])
        {
            qDebug()<<"读取完成"<<"(缓存)";
            return cache.*Member;
        }

        auto result = cred_read(key);
        qDebug()<<key<<"读取完成(io)";

        using Traits = ylt::reflection::internal::member_tratis<decltype(Member)>;
        using FieldT = Traits::value_type;

        FieldT value = {};
        if (result.size())
            memcpy(&value, result.data(), sizeof(FieldT));

        hascached[index] = true;
        return cache.*Member = value;
    }

template<class Layout, fixed_string storagename>
    template <auto Member, typename T>
    void Storage<Layout, storagename>::save(T&& value)
    {
        //只需要写缓存就好了
        const auto &index = ylt::reflection::index_of<Member>();
        auto key = keys[ylt::reflection::index_of<Member>()];
        qDebug()<<"准备写入"<<key;

        cache.*Member = value;
        hascached[index] = true;
        qDebug()<<"写入完成";
    }

#else
#include <filesystem>

template<class Layout, fixed_string storagename>
    bool Storage<Layout, storagename>::exist()
    {
        qDebug()<<"检查是否有过记录";
        qDebug()<<(held ? "存在":"无")<<"记录";
        //我们假定持有句柄文件就一定存在
        //如果损坏了那也当真不过不会进行真正的文件读写罢了
        return held ? true : std::filesystem::exists(std::string(storagename));
    }

template<class Layout, fixed_string storagename>
    Storage<Layout, storagename>::operator bool() const
    {
        return available();
    }

template <typename Layout, fixed_string storagename>
bool Storage<Layout, storagename>::available()
    {
        if (held /*!= YEW*/)return true;

        if (!exist())//不存在则创建
        {
            _file.open(storagename, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
        }
        else if (! _file.is_open())//存在但未打开
        {
            _file.open(storagename, std::ios::binary | std::ios::out | std::ios::in);
        }
        //x存在且打开：这时候应该是HAVE才对

        bool ret = (_file.is_open() && _file.good());
        held = 2*ret-1;
        return true;
    }

template<class Layout, fixed_string storagename>
    template <auto Member>
    auto Storage<Layout, storagename>::load() {
        using Traits = ylt::reflection::internal::member_tratis<decltype(Member)>;
        using Owner =  Traits::owner_type;
        using FieldT = Traits::value_type;

        constexpr auto idx = ylt::reflection::index_of<Member>();
        auto key = QString::fromUtf8(keys[ylt::reflection::index_of<Member>()]);

        qDebug()<<"准备读取"<<key;
        //bad的话就只用缓存，有没有记录过不重要
        if (held == BAD || hascached[idx])
        {
            qDebug()<<"读取完成(缓存)";
            return cache.*Member;
        }

        //HAVE但是没缓存
        FieldT value;
        _file.seekg(ylt::reflection::member_offsets<Owner>[idx]);
        _file.read(reinterpret_cast<char*>(&value), sizeof(FieldT));

        cache.*Member = value;
        hascached[idx] = true;

        qDebug()<<"读取完成(io)";
        return value;
    }

template<class Layout, fixed_string storagename>
    template <auto Member, typename T>
    void Storage<Layout, storagename>::save(T&& value) {
        using Traits = ylt::reflection::internal::member_tratis<decltype(Member)>;
        static_assert(std::is_same_v<std::decay_t<T>, typename Traits::value_type>,"value type must match field type");

        qDebug()<<"准备写入"<<ylt::reflection::name_of(Member);
        //我居然吧按偏移写改了那我最初写这个类的意义何在
        //算了天有不测风云照用吧 --25.10.11
        cache.*Member = std::forward<T>(value);
        hascached[ylt::reflection::index_of<Member>()] = true;
        qDebug()<<"写入完成";
    }

template<class Layout, fixed_string storagename>
    void Storage<Layout, storagename>::dosave()
    {
        qDebug()<<"记录落盘";
        if (held != HAVE)
            return;
        _file.seekp(0);
        _file.write((char*)&cache, sizeof(cache));
    }

template<class Layout, fixed_string storagename>
    void Storage<Layout, storagename>::clear()
    {
        qInfo()<<"清理所有记录..";
        _file.close();
        //不需要检查文件是否存在
        std::filesystem::remove(std::string(storagename));
        hascached = {};
    }

#endif