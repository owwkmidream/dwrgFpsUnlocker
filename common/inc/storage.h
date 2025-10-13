#pragma once

//todo: storage 缓存读取 & 线程锁
#include <ylt/reflection/member_value.hpp>

#include <fstream>
#include <algorithm>

#include "storage.h"

/** 存储布局 */
#pragma pack(push, 1)
    struct hipp
    {
        int fps;
        bool checked;
    };
#pragma pack(pop)

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
        return {data, N-1};
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
    //检查是否存有记录
    bool exist() const;
    //存
    template<auto Member, typename T>
    void save(T&& value);
    //取
    template<auto Member>
    auto load() const;
    //清空记录
static
    void clear();
static
    void dosave();

protected:

    //编译期生成的成员名称数组 和 成员偏移数组
    inline static const auto keys = ylt::reflection::get_member_names<Layout>();
    // inline static const auto offs = ylt::reflection::member_offsets<Layout>();
    //todo: 读写锁
    //缓存 和 字段是否为空的记录
static inline
    std::array<bool, ylt::reflection::members_count<Layout>()> hascached{};
static inline
    Layout cache{};
#ifndef GUI_BUILD_SINGLE
static inline
    std::fstream _file ;
#endif
};

template <class T>
struct StorageA : Storage<T, []{
    constexpr auto name_view = ylt::reflection::get_struct_name<T>();
    constexpr auto N = name_view.size() + 1;
    char buf[N]{};
    for (size_t i = 0; i < N - 1; ++i)
        buf[i] = name_view[i];
    buf[N - 1] = '\0';
    return fixed_string(buf);
}()>
{};

#ifdef GUI_BUILD_SINGLE

#include <qt6keychain/keychain.h>
#include <QEventLoop>
/** keychain的同步封装 */
template <typename JobType>
    struct SyncJob : public JobType
{
    SyncJob(const QString &service, QObject *parent = nullptr)
    : JobType(service, parent){}
    /*能写出这样的代码我真是炉火纯青hi啊hi啊hiahia --25.10.3*///不好一点也不好 --25.10.13
    void start(const std::function<void(QKeychain::Job*)>& afterfinish)
    {
        QEventLoop loop;
        /* 假设finished信号一定会发，已知它是Direct的
         *     那么这段lambda函数会立刻执行，将loop标记quit
         * 在同一个槽函数里，基类会登记它的deleteLater()，不过是Queued的，所以loop还没处理它就quit了
         * 所以这里应该是健壮的 */
        QObject::connect(this, &QKeychain::Job::finished, [&loop, &afterfinish, this](QKeychain::Job* job)
            {
                qDebug()<<"收到Job结束信号";
                if (afterfinish)
                    afterfinish(this);
                loop.quit();
            });
        qDebug()<<"请求Job:"<<this->key();
        JobType::start();
        loop.exec();
    }
};

template<typename Layout, fixed_string storagename>
    Storage<Layout, storagename>::operator bool()const{return true;}

template<class Layout, fixed_string storagename>
    bool Storage<Layout, storagename>::exist()const
    {
        bool vkeyexist = true;
        ylt::reflection::for_each(cache, [&vkeyexist](auto& member, auto key, auto index)
        {
            qDebug()<<"准备检查"<<key<<"["<<index<<"]";
            if (hascached[index] || vkeyexist == false)
                return;
            // 超天才QtKeychain，使我刮地三尺（搜罗野指针）😇✝  小半辈子花在这了
            //
            // ⚠️警告：要么用指针，要么别在生命周期内eventloop
            //        仅此一家的擅自deleteLater，不考虑自身是不是栈变量 --25.10.13
            auto readJobSync = new SyncJob<QKeychain::ReadPasswordJob>{QString::fromStdString(storagename)};

            readJobSync->setKey(QString::fromUtf8(key));

            readJobSync->start([&vkeyexist,index,&member](QKeychain::Job* job)
            {
                if (job->error() != QKeychain::NoError)
                {
                    vkeyexist = false;
                    qDebug()<<"job失败:"<<job->errorString();
                }
                //顺便把load给做了
                const auto& data = dynamic_cast<QKeychain::ReadPasswordJob*>(job)->binaryData();
                memcpy(&member, data.data(), sizeof(member));
                hascached[index] = true;
            });
        });

        return vkeyexist;
    }

template<class Layout, fixed_string storagename>
    void Storage<Layout, storagename>::clear()
    {
        for (auto key : keys)
        {
            auto eraseJobSync = new SyncJob<QKeychain::DeletePasswordJob>(QString::fromStdString(storagename));
            eraseJobSync->setKey(QString::fromUtf8(key));
            eraseJobSync->start({});
        }
        hascached = {};
    }

template <typename Layout, fixed_string storagename>
void Storage<Layout, storagename>::dosave()
{
    ylt::reflection::for_each(cache, [&](auto& member, auto key, auto index)
    {
        auto writeJobSync = new SyncJob<QKeychain::WritePasswordJob>(QString::fromStdString(storagename));
        writeJobSync->setKey(key.data());

        const QByteArray data( reinterpret_cast<char*>(&cache) + ylt::reflection::member_offsets<Layout>[index] , sizeof(member));
        writeJobSync->setBinaryData(data);

        writeJobSync->start(nullptr);
    });
}

template<class Layout, fixed_string storagename>
    template<auto Member>
    auto Storage<Layout, storagename>::load() const
    {
        const auto &index = ylt::reflection::index_of<Member>();
        if (hascached[index])
        {
            return cache.*Member;
        }

        SyncJob<QKeychain::ReadPasswordJob> readJobSync (QString::fromStdString(storagename));


        using Traits = ylt::reflection::internal::member_tratis<decltype(Member)>;
        using FieldT = Traits::value_type;

        auto key = QString::fromUtf8(keys[ylt::reflection::index_of<Member>()]);
        readJobSync.setKey(key);

        FieldT value = {};
        readJobSync.start([&value](QKeychain::Job* job)
        {
            if (job->error() == QKeychain::NoError)
            {
                QByteArray dat = ((QKeychain::ReadPasswordJob*)job)->binaryData();
                memcpy(&value, dat.data(), sizeof(FieldT));
            }
        });

        //返回前
        hascached[index] = true;
        return cache.*Member = value;
    }

template<class Layout, fixed_string storagename>
    template <auto Member, typename T>
    void Storage<Layout, storagename>::save(T&& value)
    {
        //只需要写缓存就好了
        const auto &index = ylt::reflection::index_of<Member>();

        cache.*Member = value;
        hascached[index] = true;
    }

#else
#include <filesystem>

template<class Layout, fixed_string storagename>
    bool Storage<Layout, storagename>::exist() const
    {
        return std::filesystem::exists(std::string(storagename));
    }

template<class Layout, fixed_string storagename>
    Storage<Layout, storagename>::operator bool() const
    {
        if (!exist())
        {
            _file.open(storagename, std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);
        }
        else if (! _file.is_open())
        {
            _file.open(storagename, std::ios::binary | std::ios::out | std::ios::in);
            return _file.is_open() && _file.good();
        }
        return _file.good();
    }

template<class Layout, fixed_string storagename>
    template <auto Member>
    auto Storage<Layout, storagename>::load() const {
        using Traits = ylt::reflection::internal::member_tratis<decltype(Member)>;
        using Owner =  Traits::owner_type;
        using FieldT = Traits::value_type;

        constexpr auto idx = ylt::reflection::index_of<Member>();

        if (hascached[idx])
            return cache.*Member;

        FieldT value;
        _file.seekg(ylt::reflection::member_offsets<Owner>[idx]);
        _file.read(reinterpret_cast<char*>(&value), sizeof(FieldT));

        return value;
    }

template<class Layout, fixed_string storagename>
    template <auto Member, typename T>
    void Storage<Layout, storagename>::save(T&& value) {
        using Traits = ylt::reflection::internal::member_tratis<decltype(Member)>;
        static_assert(std::is_same_v<std::decay_t<T>, typename Traits::value_type>,
                      "value type must match field type");

        //我居然吧按偏移写改了那我最初写这个类的意义何在
        //算了天有不测风云照用吧 --25.10.11
        cache.*Member = std::forward<T>(value);
        hascached[ylt::reflection::index_of<Member>()] = true;
    }

template<class Layout, fixed_string storagename>
    void Storage<Layout, storagename>::dosave()
    {
        //question: std::file保证内存字节序和文件字节序一样吗？
        _file.seekp(0);
        _file.write((char*)&cache, sizeof(cache));
    }

template<class Layout, fixed_string storagename>
    void Storage<Layout, storagename>::clear()
    {
        _file.close();
        std::filesystem::remove(std::string(storagename));
    }

#endif