//
// Created by Tofu on 2025/10/11.
//

#include "gamelistener.h"

#include <QMetaObject>
#include <QDebug>

#include <comdef.h>
#include <csignal>
#include <Wbemidl.h>

// Helper COM-safe Release macro
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) if((p)){ (p)->Release(); (p)=nullptr; }
#endif

// Forward: a tiny IWbemObjectSink implementation that forwards events to a Qt signal via a callback.
class WmiEventSink : public IWbemObjectSink
{
    LONG refer_count;
public:
    using Callback = std::function<void(IWbemClassObject*)>;

    explicit WmiEventSink(Callback cb) : refer_count(1), callback(std::move(cb)) {}
    virtual ~WmiEventSink() {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IWbemObjectSink)
        {
            *ppv = static_cast<IWbemObjectSink*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&refer_count); }
    STDMETHODIMP_(ULONG) Release()
    {
        ULONG u = InterlockedDecrement(&refer_count);
        if (u == 0) delete this;
        return u;
    }

    // IWbemObjectSink
    STDMETHODIMP Indicate(LONG lObjectCount, IWbemClassObject** apObjArray)
    {
        for (LONG i = 0; i < lObjectCount; ++i)
        {
            if (apObjArray[i] && callback) {
                // hand over a AddRef'd pointer to callback
                apObjArray[i]->AddRef();
                callback(apObjArray[i]);
                apObjArray[i]->Release();
            }
        }
        return WBEM_S_NO_ERROR;
    }

    STDMETHODIMP SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam, IWbemClassObject* pObjParam)
    {
        Q_UNUSED(lFlags);
        Q_UNUSED(hResult);
        Q_UNUSED(strParam);
        Q_UNUSED(pObjParam);
        // ignore for now
        return WBEM_S_NO_ERROR;
    }

private:
    Callback callback;
};

static void runWmiMonitor(const QString exeName, int pollInterval,
                          QAtomicInt* running,
                          QObject* signalEmitter /* will receive Qt signals: processStarted(), errorOccured(), started(), stopped() */)
{
    //返回值
    HRESULT hr;

    hr =
    //初始化com环境
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        QMetaObject::invokeMethod(signalEmitter, [hr, signalEmitter]() {
            QString msg = QString("CoInitializeEx failed: 0x%1").arg((unsigned long)hr, 0, 16);
            Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->errorOccured(msg);
        }, Qt::QueuedConnection);
        return;
    }

    hr =
    // 设置安全策略（进程级别，只能初始化一次）
    CoInitializeSecurity(
        nullptr,
        -1,                          // COM negotiates authentication service
        nullptr,
        nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE,
        nullptr
    );
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) { // RPC_E_TOO_LATE may occur if security already initialized; ignore it.
        QMetaObject::invokeMethod(signalEmitter, [hr, signalEmitter]() {
            QString msg = QString("CoInitializeSecurity failed: 0x%1").arg((unsigned long)hr, 0, 16);
            Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->errorOccured(msg);
        }, Qt::QueuedConnection);
        CoUninitialize();
        return;
    }

    IWbemLocator* pLoc = nullptr; //WMI入口对象
    IWbemServices* pSvc = nullptr; // WMI服务端口
    hr =

    CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) {
        QMetaObject::invokeMethod(signalEmitter, [hr, signalEmitter]() {
            QString msg = QString("CoCreateInstance(WbemLocator) failed: 0x%1").arg((unsigned long)hr, 0, 16);
            Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->errorOccured(msg);
        }, Qt::QueuedConnection);
        CoUninitialize();
        return;
    }

    // Connect to the root\cimv2 namespace
    hr = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        nullptr,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        &pSvc
    );
    if (FAILED(hr)) {
        QMetaObject::invokeMethod(signalEmitter, [hr, signalEmitter]() {
            QString msg = QString("ConnectServer failed: 0x%1").arg((unsigned long)hr, 0, 16);
            Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->errorOccured(msg);
        }, Qt::QueuedConnection);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return;
    }

    // Set security levels on the proxy
    hr = CoSetProxyBlanket(
        pSvc,                        // the proxy to set
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE
    );
    if (FAILED(hr)) {
        QMetaObject::invokeMethod(signalEmitter, [hr, signalEmitter]() {
            QString msg = QString("CoSetProxyBlanket failed: 0x%1").arg((unsigned long)hr, 0, 16);
            Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->errorOccured(msg);
        }, Qt::QueuedConnection);
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return;
    }

    // Build WQL notification query for process creation filtered by exe name
    QString q = QString("SELECT * FROM __InstanceCreationEvent WITHIN %1 WHERE TargetInstance ISA 'Win32_Process' AND TargetInstance.Name = '%2'")
                    .arg(pollInterval)
                    .arg(exeName);

    // Create sink: when WMI indicates an object, the sink callback will parse the TargetInstance and emit a Qt signal
    WmiEventSink* sink = new WmiEventSink([signalEmitter](IWbemClassObject* obj) {
        // obj is the event object (e.g., __InstanceCreationEvent), we need to get TargetInstance, then get ProcessId and Name
        VARIANT varInstance;
        VariantInit(&varInstance);

        HRESULT hr = obj->Get(L"TargetInstance", 0, &varInstance, nullptr, nullptr);
        if (SUCCEEDED(hr) && varInstance.vt == VT_UNKNOWN && varInstance.punkVal) {
            IWbemClassObject* pTarget = nullptr;
            hr = varInstance.punkVal->QueryInterface(IID_IWbemClassObject, (void**)&pTarget);
            if (SUCCEEDED(hr) && pTarget) {
                VARIANT vPid;
                VariantInit(&vPid);
                VARIANT vName;
                VariantInit(&vName);

                if (SUCCEEDED(pTarget->Get(L"ProcessId", 0, &vPid, nullptr, nullptr)) && (vPid.vt == VT_I4 || vPid.vt == VT_UI4)) {
                    DWORD pid = vPid.ulVal;

                    // emit Qt signal (queued connection automatically since different thread)
                    QMetaObject::invokeMethod(signalEmitter, [pid, signalEmitter]() {
                        Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->processStarted(pid);
                    }, Qt::QueuedConnection);
                }

                VariantClear(&vPid);
                VariantClear(&vName);
                pTarget->Release();
            }
        }
        VariantClear(&varInstance);
    });

    // Ask WMI to execute the async notification query
    hr = pSvc->ExecNotificationQueryAsync(
        _bstr_t(L"WQL"),
        _bstr_t(q.toStdWString().c_str()),
        WBEM_FLAG_SEND_STATUS,
        nullptr,
        sink
    );
    if (FAILED(hr)) {
        QMetaObject::invokeMethod(signalEmitter, [hr, signalEmitter]() {
            QString msg = QString("ExecNotificationQueryAsync failed: 0x%1").arg((unsigned long)hr, 0, 16);
            Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->errorOccured(msg);
        }, Qt::QueuedConnection);
        // cleanup
        sink->Release();
        SAFE_RELEASE(pSvc);
        SAFE_RELEASE(pLoc);
        CoUninitialize();
        return;
    }

    // notify started
    QMetaObject::invokeMethod(signalEmitter, [signalEmitter]() {
        Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->started();
    }, Qt::QueuedConnection);

    //循环直至running is false
    while (running->loadAcquire()) {
        // Sleep small amount to be responsive to stop() calls.
        // The WMI async query will deliver events to our sink regardless.
        Sleep(250);
    }

    // cancellation: tell WMI to cancel async delivery for our sink
    // IWbemServices::CancelAsync takes IWbemObjectSink*
    // pSvc->CancelAsync(sink);
    pSvc->CancelAsyncCall(sink);

    // cleanup
    sink->Release();
    SAFE_RELEASE(pSvc);
    SAFE_RELEASE(pLoc);

    // uninitialize COM for this thread
    CoUninitialize();

    QMetaObject::invokeMethod(signalEmitter, [signalEmitter]() {
        Q_EMIT static_cast<ProgStartListener*>(signalEmitter)->stopped();
    }, Qt::QueuedConnection);
}

ProgStartListener::ProgStartListener(QObject* parent)
    : QObject(parent),
      m_running(false)
{
    // nothing heavy in ctor
}

ProgStartListener::~ProgStartListener()
{
    stop();
    // m_workerThread.quit();
    // m_workerThread.wait();
}

void ProgStartListener::start(const QString& exeName, int interval)
{
    //如果已经启动了
    if (m_running.loadAcquire()) return;
    m_running.storeRelease(true);

    // Move a small QObject (this) to the worker thread so we can use it safely for queued signal invocations.
    // Actually we will invoke QMetaObject::invokeMethod with the same object (this).
    // Start a thread and run the blocking loop inside it via QtConcurrent or lambda.
    // We'll use QThread with lambda
    // 创建一个线程，调用runWmiMonitor，填入当前的类记录的变量
    // auto func = std::bind(runWmiMonitor, std::placeholders::_1, std::placeholders::_2, &m_running, this);
    QThread* thread = QThread::create([this, &exeName, interval]() {
        runWmiMonitor(exeName, interval, &m_running, this);
    });
    // QThread* thread2 = QThread::create(func, exeName, interval);
    thread->start();

    // 设置线程的对象名称
    thread->setObjectName(QStringLiteral("ProgStartListenerWorker"));
    // We attach this thread to m_workerThread for lifecycle control
    //question: 主线程创建的对象moveto主线程是多此一举的
    //question: m_workerThread好像根本没有用
    // m_workerThread.moveToThread(QThread::currentThread()); // just ensure it's valid; we'll keep pointer manually
    //记录线程
    // keep the pointer by parenting to this object so it won't leak
    m_threads.append(thread);
}

void ProgStartListener::stop()
{
    if (!m_running.loadAcquire()) return;
    m_running.storeRelease(false);

    for (QThread* t : m_threads) {
        if (t) {
            /// 发出退出信号并等待，如果还是没有退出就强行terminate
            t->quit();
            t->wait(2000);
            // if still running, terminate (last resort)
            if (t->isRunning()) {
                t->terminate();
                t->wait(1000);
            }
        }
    }
}
