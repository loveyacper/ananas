
#include "EventLoop.h"

#include <cassert>
#include <thread>

#include "Acceptor.h"
#include "Connection.h"
#include "Connector.h"

#if defined(__APPLE__)
    #include "Kqueue.h"
#elif defined(__gnu_linux__)
    #include "Epoller.h"
#else
    #error "Only support osx and linux"
#endif

#include "AnanasDebug.h"
#include "Util.h"

namespace ananas
{

EventLoop::EventLoop() : stop_(false)
{
#if defined(__APPLE__)
    poller_.reset(new internal::Kqueue); 
#elif defined(__gnu_linux__)
    poller_.reset(new internal::Epoller); 
#else
    #error "Only support osx and linux"
#endif

    internal::InitDebugLog(logALL);
}

EventLoop::~EventLoop()
{
}
    
    
bool EventLoop::Listen(const char* ip, uint16_t hostPort,
                       NewConnCallback newConnCallback)
{
    SocketAddr  addr;
    addr.Init(ip, hostPort);

    return Listen(addr, std::move(newConnCallback));
}
    
bool EventLoop::Listen(const SocketAddr& listenAddr,
                       NewConnCallback newConnCallback)
{
    using internal::Acceptor;

    std::unique_ptr<Acceptor> s(new Acceptor(this));
    s->SetNewConnCallback(std::move(newConnCallback));
    if (!s->Bind(listenAddr))
        return false;
    
    s.release();
    return true;
}

bool EventLoop::Connect(const char* ip, uint16_t hostPort, NewConnCallback nccb, ConnFailCallback cfcb)
{
    SocketAddr  addr;
    addr.Init(ip, hostPort);

    return Connect(addr, nccb, cfcb);
}


bool EventLoop::Connect(const SocketAddr& dst, NewConnCallback nccb, ConnFailCallback cfcb)
{
    using internal::Connector;

    std::unique_ptr<Connector> cli(new Connector(this));
    cli->SetFailCallback(cfcb);
    cli->SetNewConnCallback(nccb);

    if (!cli->Connect(dst))
        return false;

    if (cli->State() == internal::ConnectState::connecting)
        cli.release();

    return true;
}

thread_local unsigned int EventLoop::s_id = 0;

bool EventLoop::Register(int events, internal::EventSource* src)
{
    if (events == 0)
        return false;

    if (src->GetUniqueId() != 0)
        assert(false);

    ++ s_id;
    if (s_id == 0) // wrap around
        s_id = 1;

    src->SetUniqueId(s_id);

    if (poller_->Register(src->Identifier(), events, src))
        return eventSourceSet_.insert(std::make_pair(src->GetUniqueId(), std::unique_ptr<internal::EventSource>(src))).second;

    return false;
}

bool EventLoop::Modify(int events, internal::EventSource* src)
{
    assert (eventSourceSet_.count(src->GetUniqueId()));
    return poller_->Modify(src->Identifier(), events, src);
}

void EventLoop::Unregister(int events, internal::EventSource* src)
{
    auto tmp = src->Identifier();
    size_t nTask = eventSourceSet_.erase(src->GetUniqueId());
    if (1 != nTask)
    {
        ERR(internal::g_debug) << "Can not find socket " << tmp;
        assert (false);
    }
    else
    {
        poller_->Unregister(tmp, events);
    }
}

bool EventLoop::Cancel(TimerId id)
{
    return timers_.Cancel(id);
}

void EventLoop::Run()
{
    while (!stop_)
    {
        const DurationMs kDefaultPollTime(10);
        const DurationMs kMinPollTime(1);

        auto timeout = std::min(kDefaultPollTime, timers_.NearestTimer());
        timeout = std::max(kMinPollTime, timeout);

        Loop(timeout);
    }
}

bool EventLoop::Loop(DurationMs timeout)
{
    DEFER {
        decltype(functors_) tmp;
        tmp.swap(functors_);

        for (const auto& f : tmp)
            f();
    };

    timers_.Update();

    if (eventSourceSet_.empty())
    {
        std::this_thread::sleep_for(timeout);
        return false;
    }

    const int ready = poller_->Poll(static_cast<int>(eventSourceSet_.size()), static_cast<int>(timeout.count()));
    const auto& fired = poller_->GetFiredEvents();

    for (int i = 0; i < ready; ++ i)
    {
        auto src = (internal::EventSource* )fired[i].userdata;

        if (fired[i].events & internal::eET_Read) 
        {
            if (!src->HandleReadEvent())
            {
                src->HandleErrorEvent();
                continue;
            }
        }

        if (fired[i].events & internal::eET_Write)
        {
            if (!src->HandleWriteEvent())
            {
                src->HandleErrorEvent();
                continue;
            }
        }
        
        if (fired[i].events & internal::eET_Error)
        {
            ERR(internal::g_debug) << "eET_Error";
            src->HandleErrorEvent();
            continue;
        }
    }

    return ready >= 0;
}


} // end namespace ananas

