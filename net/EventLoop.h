
#ifndef BERT_EVENTLOOP_H
#define BERT_EVENTLOOP_H

#include <map>
#include <memory>
#include <sys/resource.h>

#include "Timer.h"
#include "Poller.h"
#include "Typedefs.h"
#include "util/Scheduler.h"
#include "future/Future.h"

namespace ananas
{

struct SocketAddr;
class  Connection;
class EventLoopGroup;

namespace internal
{
class  Connector;
}

class EventLoop : public Scheduler
{
public:
    explicit
    EventLoop(EventLoopGroup* group);
    ~EventLoop();

    EventLoop(const EventLoop& ) = delete;
    void operator= (const EventLoop& ) = delete;
    EventLoop(EventLoop&& ) = delete;
    void operator= (EventLoop&& ) = delete;

    // listener
    bool Listen(const SocketAddr& listenAddr, NewTcpConnCallback cb);
    bool Listen(const char* ip, uint16_t hostPort, NewTcpConnCallback cb);
    bool ListenUDP(const SocketAddr& listenAddr,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb);
    bool ListenUDP(const char* ip,
                   uint16_t hostPort,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb);

    // udp client
    bool CreateClientUDP(UDPMessageCallback mcb,
                         UDPCreateCallback ccb);


    // connector 
    bool Connect(const SocketAddr& dst,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max());
    bool Connect(const char* ip,
                 uint16_t hostPort,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max());

    // timer : NOT thread-safe
    template <int RepeatCount = 1, typename Duration, typename F, typename... Args>
    TimerId ScheduleAfter(const Duration& duration, F&& f, Args&&... args);
    bool Cancel(TimerId id);

    // sleep
    Future<void> Sleep(std::chrono::milliseconds dur);

    // for future : thread-safe
    void ScheduleOnceAfter(std::chrono::milliseconds duration, std::function<void ()> f) override;
    void ScheduleOnce(std::function<void ()> f) override;

    // thread-safe
    template <typename F, typename... Args>
    void Execute(F&& f, Args&&... args);

    void Run();
    bool Loop(DurationMs timeout);

    bool Register(int events, std::shared_ptr<internal::Channel> src);
    bool Modify(int events, std::shared_ptr<internal::Channel> src);
    void Unregister(int events, std::shared_ptr<internal::Channel> src);

    std::size_t Size() const { return channelSet_.size(); }
    bool IsInSameLoop() const;
    EventLoopGroup* Parent() const { return group_; }

    static EventLoop* GetCurrentEventLoop();

    static void SetMaxOpenFd(rlim_t maxfdPlus1);

private:
    EventLoopGroup* group_;
    static bool s_exit;

    std::map<unsigned int, std::shared_ptr<internal::Channel> > channelSet_;
    std::unique_ptr<internal::Poller>  poller_;

    std::mutex timerMutex_;
    internal::TimerManager timers_;
        
    std::mutex fctrMutex_;
    std::vector<std::function<void ()> > functors_;
    
    static thread_local unsigned int s_id;

    // max open fd + 1
    static rlim_t s_maxOpenFdPlus1;
};


template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId EventLoop::ScheduleAfter(const Duration& duration, F&& f, Args&&... args)
{
    // not thread-safe for now, consider use future.
    assert (IsInSameLoop());
    return timers_.ScheduleAfter<RepeatCount>(duration,
                                              std::forward<F>(f),
                                              std::forward<Args>(args)...);
}

template <typename F, typename... Args>
void EventLoop::Execute(F&& f, Args&&... args)
{
    if (IsInSameLoop())
    {
        std::forward<F>(f)(std::forward<Args>(args)...);
    }
    else
    {
        auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto func = [temp]() { (void)temp(); };

        std::unique_lock<std::mutex> guard(fctrMutex_);
        functors_.emplace_back(std::move(func));
    }
}

} // end namespace ananas

#endif

