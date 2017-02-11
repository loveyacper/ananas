
#ifndef BERT_EVENTLOOP_H
#define BERT_EVENTLOOP_H

#include <map>
#include <memory>
#include <sys/resource.h>

#include "Timer.h"
#include "Poller.h"
#include "Typedefs.h"
#include "util/Scheduler.h"

namespace ananas
{

struct SocketAddr;
class  Connection;

namespace internal
{
class  Connector;
}

class EventLoop : public Scheduler
{
public:
    EventLoop();
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
    bool ListenUDP(const char* ip, uint16_t hostPort,
            UDPMessageCallback mcb,
            UDPCreateCallback ccb);

    // udp client
    bool CreateClientUDP(UDPMessageCallback mcb,
                         UDPCreateCallback ccb);


    // connector 
    bool Connect(const SocketAddr& dst, NewTcpConnCallback nccb, TcpConnFailCallback cfcb, DurationMs timeout = DurationMs::max());
    bool Connect(const char* ip, uint16_t hostPort, NewTcpConnCallback nccb, TcpConnFailCallback cfcb, DurationMs timeout = DurationMs::max());

    // TODO socketpair

    // timer
    template <int RepeatCount = 1, typename Duration, typename F, typename... Args>
    TimerId ScheduleAfter(const Duration& duration, F&& f, Args&&... args);
    bool Cancel(TimerId id);

    // for future
    void ScheduleOnceAfter(std::chrono::milliseconds duration, std::function<void ()> f) override;
    void ScheduleOnce(std::function<void ()> f) override;

    // 
    template <typename F, typename... Args>
    void ScheduleNextTick(F&& f, Args&&... args);

    bool IsStop() const { return stop_; }
    void Stop()         { stop_ = true; }

    void Run();
    bool Loop(DurationMs timeout);

    bool Register(int events, internal::EventSource* src);
    bool Modify(int events, internal::EventSource* src);
    void Unregister(int events, internal::EventSource* src);

    std::size_t Size() const { return eventSourceSet_.size(); }

    static void ExitApplication();

    static void SetMaxOpenFd(rlim_t maxfdPlus1);

private:
    bool stop_;
    static bool s_exit;

    std::map<unsigned int, std::unique_ptr<internal::EventSource> > eventSourceSet_;
    std::unique_ptr<internal::Poller>  poller_;

    internal::TimerManager timers_;
        
    std::vector<std::function<void ()> > functors_;
    
    static thread_local unsigned int s_id;

    // max open fd + 1
    static rlim_t s_maxOpenFdPlus1;
};


template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId EventLoop::ScheduleAfter(const Duration& duration, F&& f, Args&&... args)
{
    return timers_.ScheduleAfter<RepeatCount>(duration, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename F, typename... Args>
void EventLoop::ScheduleNextTick(F&& f, Args&&... args)
{
    auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto func = [temp]() { (void)temp(); };
    functors_.emplace_back(std::move(func));
}


} // end namespace ananas

#endif

