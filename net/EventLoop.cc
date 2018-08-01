
#include <cassert>
#include <thread>

#include "EventLoop.h"
#include "EventLoopGroup.h"

#include "Acceptor.h"
#include "Connection.h"
#include "Connector.h"
#include "DatagramSocket.h"

#if defined(__APPLE__)
#include "Kqueue.h"
#elif defined(__gnu_linux__)
#include "Epoller.h"
#else
#error "Only support osx and linux"
#endif

#include "AnanasDebug.h"
#include "util/Util.h"

namespace ananas {

static thread_local EventLoop* g_thisLoop;

EventLoop* EventLoop::GetCurrentEventLoop() {
    return g_thisLoop;
}

void EventLoop::SetMaxOpenFd(rlim_t maxfdPlus1) {
    if (ananas::SetMaxOpenFd(maxfdPlus1))
        s_maxOpenFdPlus1 = maxfdPlus1;
}

EventLoop::EventLoop(internal::EventLoopGroup* group) :
    group_(group) {
    assert (!g_thisLoop && "There must be only one EventLoop per thread");
    g_thisLoop = this;

    internal::InitDebugLog(logALL);

#if defined(__APPLE__)
    poller_.reset(new internal::Kqueue);
#elif defined(__gnu_linux__)
    poller_.reset(new internal::Epoller);
#else
#error "Only support mac os and linux"
#endif

    notifier_ = std::make_shared<internal::PipeChannel>();
    id_ = s_evId ++;
}

EventLoop::~EventLoop() {
}

bool EventLoop::Listen(const char* ip,
                       uint16_t hostPort,
                       NewTcpConnCallback newConnCallback) {
    SocketAddr addr;
    addr.Init(ip, hostPort);

    return Listen(addr, std::move(newConnCallback));
}

bool EventLoop::Listen(const SocketAddr& listenAddr,
                       NewTcpConnCallback newConnCallback) {
    using internal::Acceptor;

    auto s = std::make_shared<Acceptor>(this);
    s->SetNewConnCallback(std::move(newConnCallback));
    if (!s->Bind(listenAddr))
        return false;

    return true;
}

bool EventLoop::ListenUDP(const SocketAddr& listenAddr,
                          UDPMessageCallback mcb,
                          UDPCreateCallback ccb) {
    auto s = std::make_shared<DatagramSocket>(this);
    s->SetMessageCallback(mcb);
    s->SetCreateCallback(ccb);
    if (!s->Bind(&listenAddr))
        return false;

    return true;
}

bool EventLoop::ListenUDP(const char* ip, uint16_t hostPort,
                          UDPMessageCallback mcb,
                          UDPCreateCallback ccb) {
    SocketAddr addr;
    addr.Init(ip, hostPort);

    return ListenUDP(addr, mcb, ccb);
}


bool EventLoop::CreateClientUDP(UDPMessageCallback mcb,
                                UDPCreateCallback ccb) {
    auto s = std::make_shared<DatagramSocket>(this);
    s->SetMessageCallback(mcb);
    s->SetCreateCallback(ccb);
    if (!s->Bind(nullptr))
        return false;

    return true;
}

bool EventLoop::Connect(const char* ip,
                        uint16_t hostPort,
                        NewTcpConnCallback nccb,
                        TcpConnFailCallback cfcb,
                        DurationMs timeout,
                        EventLoop* dstLoop) {
    SocketAddr addr;
    addr.Init(ip, hostPort);

    return Connect(addr, nccb, cfcb, timeout, dstLoop);
}


bool EventLoop::Connect(const SocketAddr& dst,
                        NewTcpConnCallback nccb,
                        TcpConnFailCallback cfcb,
                        DurationMs timeout,
                        EventLoop* dstLoop) {
    using internal::Connector;

    auto cli = std::make_shared<Connector>(this);
    cli->SetFailCallback(cfcb);
    cli->SetNewConnCallback(nccb);

    if (!cli->Connect(dst, timeout, dstLoop))
        return false;

    return true;
}


thread_local unsigned int EventLoop::s_id = 0;

std::atomic<int> EventLoop::s_evId {0};

rlim_t EventLoop::s_maxOpenFdPlus1 = ananas::GetMaxOpenFd();

bool EventLoop::Register(int events, std::shared_ptr<internal::Channel> src) {
    if (events == 0)
        return false;

    if (src->GetUniqueId() != 0)
        assert(false);

    /* man getrlimit:
     * RLIMIT_NOFILE
     * Specifies a value one greater than the maximum file descriptor number
     * that can be opened by this process.
     * Attempts (open(2), pipe(2), dup(2), etc.)  to exceed this limit yield the error EMFILE.
     */
    if (src->Identifier() + 1 >= static_cast<int>(s_maxOpenFdPlus1)) {
        ANANAS_ERR
                << "Register failed! Max open fd " << s_maxOpenFdPlus1
                << ", current fd " << src->Identifier();

        return false;
    }

    ++ s_id;
    if (s_id == 0) // wrap around
        s_id = 1;

    src->SetUniqueId(s_id);
    ANANAS_INF << "Register " << s_id << " to me " << pthread_self();

    if (poller_->Register(src->Identifier(), events, src.get()))
        return channelSet_.insert({src->GetUniqueId(), src}).second;

    return false;
}

bool EventLoop::Modify(int events, std::shared_ptr<internal::Channel> src) {
    assert (channelSet_.count(src->GetUniqueId()));
    return poller_->Modify(src->Identifier(), events, src.get());
}

void EventLoop::Unregister(int events, std::shared_ptr<internal::Channel> src) {
    const int fd = src->Identifier();
    ANANAS_INF << "Unregister socket id " << fd;
    poller_->Unregister(fd, events);

    size_t nTask = channelSet_.erase(src->GetUniqueId());
    if (nTask != 1) {
        ANANAS_ERR << "Can not find socket id " << fd;
        assert (false);
    }
}

bool EventLoop::Cancel(TimerId id) {
    return timers_.Cancel(id);
}

void EventLoop::Run() {
    const DurationMs kDefaultPollTime(10);
    const DurationMs kMinPollTime(1);

    Register(internal::eET_Read, notifier_);

    while (!group_->IsStopped()) {
        auto timeout = std::min(kDefaultPollTime, timers_.NearestTimer());
        timeout = std::max(kMinPollTime, timeout);

        Loop(timeout);
    }

    for (auto& kv : channelSet_) {
        poller_->Unregister(internal::eET_Read | internal::eET_Write,
                            kv.second->Identifier());
    }

    channelSet_.clear();
    poller_.reset();
}

bool EventLoop::Loop(DurationMs timeout) {
    ANANAS_DEFER {
        timers_.Update();

        // do not block
        if (fctrMutex_.try_lock()) {
            // Use tmp : if f add callback to functors_
            decltype(functors_) funcs;
            funcs.swap(functors_);
            fctrMutex_.unlock();

            for (const auto& f : funcs)
                f();
        }
    };

    if (channelSet_.empty()) {
        std::this_thread::sleep_for(timeout);
        return false;
    }

    const int ready = poller_->Poll(static_cast<int>(channelSet_.size()),
                                    static_cast<int>(timeout.count()));
    if (ready < 0)
        return false;

    const auto& fired = poller_->GetFiredEvents();

    // Consider stale event, DO NOT unregister another socket in your event handler!

    std::vector<std::shared_ptr<internal::Channel>> sources(ready);
    for (int i = 0; i < ready; ++ i) {
        auto src = (internal::Channel* )fired[i].userdata;
        sources[i] = src->shared_from_this();

        if (fired[i].events & internal::eET_Read) {
            if (!src->HandleReadEvent()) {
                src->HandleErrorEvent();
            }
        }

        if (fired[i].events & internal::eET_Write) {
            if (!src->HandleWriteEvent()) {
                src->HandleErrorEvent();
            }
        }

        if (fired[i].events & internal::eET_Error) {
            ANANAS_ERR << "eET_Error for " << src->Identifier();
            src->HandleErrorEvent();
        }
    }

    return ready >= 0;
}

bool EventLoop::IsInSameLoop() const {
    return this == g_thisLoop;
}

void EventLoop::ScheduleAfter(std::chrono::milliseconds duration,
                              std::function<void()> f) {
    if (IsInSameLoop())
        ScheduleAfterWithRepeat<1>(duration, std::move(f));
    else
        Execute([=]() {
        ScheduleAfterWithRepeat<1>(duration, std::move(f));
    });
}

void EventLoop::Schedule(std::function<void()> f) {
    Execute(std::move(f));
}

} // end namespace ananas

