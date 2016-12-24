
#ifndef BERT_EVENTLOOP_H
#define BERT_EVENTLOOP_H

#include <map>
#include <memory>
#include <functional>
#include <sys/resource.h>
#include "Timer.h"
#include "Poller.h"
#include "DatagramSocket.h"
#include "util/ThreadLocalSingleton.h"
#include "util/TimeScheduler.h"

namespace ananas
{

struct SocketAddr;
class  Connection;

namespace internal
{
class  Connector;
}

#define ANANAS_EVENTLOOP (ananas::g_eventloop.Instance())
#define ANANAS_EVENTLOOP_PTR (&ananas::g_eventloop.Instance())

class EventLoop : public TimeScheduler // for future timeout
{
public:
    using NewConnCallback = std::function<void (Connection* )>;
    using ConnFailCallback = std::function<void (EventLoop*, const SocketAddr& peer)>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop& ) = delete;
    void operator= (const EventLoop& ) = delete;
    EventLoop(EventLoop&& ) = delete;
    void operator= (EventLoop&& ) = delete;

    // listener
    bool Listen(const SocketAddr& listenAddr, NewConnCallback cb);
    bool Listen(const char* ip, uint16_t hostPort, NewConnCallback cb);
    bool ListenUDP(const SocketAddr& listenAddr,
            DatagramSocket::MessageCallback mcb,
            DatagramSocket::CreateCallback ccb);
    bool ListenUDP(const char* ip, uint16_t hostPort,
            DatagramSocket::MessageCallback mcb,
            DatagramSocket::CreateCallback ccb);

    // udp client
    bool CreateClientUDP(DatagramSocket::MessageCallback mcb,
                         DatagramSocket::CreateCallback ccb);


    // connector 
    bool Connect(const SocketAddr& dst, NewConnCallback nccb, ConnFailCallback cfcb);
    bool Connect(const char* ip, uint16_t hostPort, NewConnCallback nccb, ConnFailCallback cfcb);

    // timer
    template <int RepeatCount = 1, typename Duration, typename F, typename... Args>
    TimerId ScheduleAfter(const Duration& duration, F&& f, Args&&... args);
    bool Cancel(TimerId id);

    // for future
    void ScheduleOnceAfter(std::chrono::milliseconds duration, std::function<void()> f) override;

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

    static void ExitAll();

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

extern ThreadLocalSingleton<EventLoop> g_eventloop; // Singleton per thread

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

