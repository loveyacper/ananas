
#include "EventLoop.h"

#include <cassert>
#include <thread>
#include <signal.h>

#include "Acceptor.h"
#include "Connection.h"
#include "Connector.h"
#include "DatagramSocket.h"
#include "ThreadPool.h"

#if defined(__APPLE__)
    #include "Kqueue.h"
#elif defined(__gnu_linux__)
    #include "Epoller.h"
#else
    #error "Only support osx and linux"
#endif

#include "AnanasDebug.h"
#include "AnanasLogo.h"
#include "util/Util.h"

static void SignalHandler(int num)
{
    ananas::EventLoop::ExitApplication();
}
    
static std::once_flag s_signalInit;

static void InitSignal()
{
    struct sigaction sig;
    ::memset(&sig, 0, sizeof(sig));
   
    sig.sa_handler = SignalHandler;
    sigaction(SIGINT, &sig, NULL);
                                  
    // ignore sigpipe
    sig.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig, NULL);

#ifdef ANANAS_LOGO
    // logo
    printf("%s\n", ananas::internal::logo);
#endif
}


namespace ananas
{


bool EventLoop::s_exit = false;
    
void EventLoop::ExitApplication()
{
    s_exit = true;
}

void EventLoop::SetMaxOpenFd(rlim_t maxfdPlus1)
{
    if (ananas::SetMaxOpenFd(maxfdPlus1))
        s_maxOpenFdPlus1 = maxfdPlus1;
}

EventLoop::EventLoop() : stop_(false)
{
#if defined(__APPLE__)
    poller_.reset(new internal::Kqueue); 
#elif defined(__gnu_linux__)
    poller_.reset(new internal::Epoller); 
#else
    #error "Only support mac os and linux"
#endif

    internal::InitDebugLog(logALL);
}

EventLoop::~EventLoop()
{
}
    
    
bool EventLoop::Listen(const char* ip, uint16_t hostPort,
                       NewTcpConnCallback newConnCallback)
{
    std::string realIp = ConvertIp(ip);
        
    SocketAddr addr;
    addr.Init(realIp.c_str(), hostPort);

    return Listen(addr, std::move(newConnCallback));
}
    
bool EventLoop::Listen(const SocketAddr& listenAddr,
                       NewTcpConnCallback newConnCallback)
{
    using internal::Acceptor;

    std::unique_ptr<Acceptor> s(new Acceptor(this));
    s->SetNewConnCallback(std::move(newConnCallback));
    if (!s->Bind(listenAddr))
        return false;
    
    s.release();
    return true;
}

bool EventLoop::ListenUDP(const SocketAddr& listenAddr,
        UDPMessageCallback mcb,
        UDPCreateCallback ccb)
{
    std::unique_ptr<DatagramSocket> s(new DatagramSocket(this));
    s->SetMessageCallback(mcb);
    s->SetCreateCallback(ccb);
    if (!s->Bind(&listenAddr))
        return false;
    
    s.release();
    return true;
}

bool EventLoop::ListenUDP(const char* ip, uint16_t hostPort,
        UDPMessageCallback mcb,
        UDPCreateCallback ccb)
{
    std::string realIp = ConvertIp(ip);
        
    SocketAddr addr;
    addr.Init(realIp.c_str(), hostPort);

    return ListenUDP(addr, mcb, ccb);
}


bool EventLoop::CreateClientUDP(UDPMessageCallback mcb,
                                UDPCreateCallback ccb)
{
    std::unique_ptr<DatagramSocket> s(new DatagramSocket(this));
    s->SetMessageCallback(mcb);
    s->SetCreateCallback(ccb);
    if (!s->Bind(nullptr))
        return false;
    
    s.release();
    return true;
}

bool EventLoop::Connect(const char* ip, uint16_t hostPort, NewTcpConnCallback nccb, TcpConnFailCallback cfcb)
{
    std::string realIp = ConvertIp(ip);
        
    SocketAddr addr;
    addr.Init(realIp.c_str(), hostPort);

    return Connect(addr, nccb, cfcb);
}


bool EventLoop::Connect(const SocketAddr& dst, NewTcpConnCallback nccb, TcpConnFailCallback cfcb)
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

rlim_t EventLoop::s_maxOpenFdPlus1 = ananas::GetMaxOpenFd();

bool EventLoop::Register(int events, internal::EventSource* src)
{
    if (events == 0)
        return false;

    if (src->GetUniqueId() != 0)
        assert(false);

    /* man getrlimit:
     * RLIMIT_NOFILE
     * Specifies a value one greater than the maximum file descriptor number that can be opened by this process.
     * Attempts (open(2), pipe(2), dup(2), etc.)  to exceed this limit yield the error EMFILE.
     */
    if (src->Identifier() + 1 >= static_cast<int>(s_maxOpenFdPlus1))
    {
        ERR(internal::g_debug)
            << "Register failed! Max open fd " << s_maxOpenFdPlus1
            << ", current fd " << src->Identifier();

        return false;
    }

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
    const int fd = src->Identifier();
    INF(internal::g_debug) << "Unregister socket id " << fd;
    poller_->Unregister(fd, events);

    size_t nTask = eventSourceSet_.erase(src->GetUniqueId());
    if (nTask != 1)
    {
        ERR(internal::g_debug) << "Can not find socket id " << fd;
        assert (false);
    }
}

bool EventLoop::Cancel(TimerId id)
{
    return timers_.Cancel(id);
}

void EventLoop::Run()
{
    std::call_once(s_signalInit, InitSignal);

    while (!stop_ && !s_exit)
    {
        const DurationMs kDefaultPollTime(10);
        const DurationMs kMinPollTime(1);

        auto timeout = std::min(kDefaultPollTime, timers_.NearestTimer());
        timeout = std::max(kMinPollTime, timeout);

        Loop(timeout);
    }
        
    for (auto& kv : eventSourceSet_)
    {
        poller_->Unregister(internal::eET_Read | internal::eET_Write, kv.second->Identifier());
    }

    eventSourceSet_.clear();
    poller_.reset();

    if (s_exit)
    {
        // thread safe exit
        LogManager::Instance().Stop();
        ThreadPool::Instance().JoinAll();
    }
}

bool EventLoop::Loop(DurationMs timeout)
{
    ANANAS_DEFER
    {
        timers_.Update();

        // Use tmp : if f add callback to functors_
        decltype(functors_) tmp;
        tmp.swap(functors_);

        for (const auto& f : tmp)
            f();
    };

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
            ERR(internal::g_debug) << "eET_Error for " << src->Identifier();
            src->HandleErrorEvent();
            continue;
        }
    }

    return ready >= 0;
}

    
void EventLoop::ScheduleOnceAfter(std::chrono::milliseconds duration, std::function<void()> f)
{
    this->ScheduleAfter<1>(duration, std::move(f));
}

void EventLoop::ScheduleOnce(std::function<void()> f)
{
    this->ScheduleNextTick(std::move(f));
}

} // end namespace ananas

