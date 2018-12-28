
#ifndef BERT_EVENTLOOP_H
#define BERT_EVENTLOOP_H

#include <map>
#include <memory>
#include <sys/resource.h>

#include "Poller.h"
#include "PipeChannel.h"
#include "Typedefs.h"
#include "ananas/util/Timer.h"
#include "ananas/util/Scheduler.h"
#include "ananas/future/Future.h"

namespace ananas {

struct SocketAddr;

namespace internal {
class Connector;
class EventLoopGroup;
}

// One thread should at most has one eventLoop object.
//
class EventLoop : public Scheduler {
public:
    explicit
    EventLoop(internal::EventLoopGroup* group);
    ~EventLoop();

    EventLoop(const EventLoop& ) = delete;
    void operator= (const EventLoop& ) = delete;
    EventLoop(EventLoop&& ) = delete;
    void operator= (EventLoop&& ) = delete;

    // listener
    bool Listen(const SocketAddr& addr, NewTcpConnCallback cb);
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
                 DurationMs timeout = DurationMs::max(),
                 EventLoop* dstLoop = nullptr);
    bool Connect(const char* ip,
                 uint16_t hostPort,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max(),
                 EventLoop* dstLoop = nullptr);

    // timer : NOT thread-safe
    // See `Timer::ScheduleAtWithRepeat`
    template <int RepeatCount, typename Duration, typename F, typename... Args>
    TimerId ScheduleAtWithRepeat(const TimePoint& , const Duration& , F&& , Args&&...);
    // See `Timer::ScheduleAfterWithRepeat`
    template <int RepeatCount, typename Duration, typename F, typename... Args>
    TimerId ScheduleAfterWithRepeat(const Duration& , F&& , Args&&...);
    bool Cancel(TimerId id);

    // See `Timer::ScheduleAt`
    template <typename F, typename... Args>
    TimerId ScheduleAt(const TimePoint& , F&& , Args&&...);

    // See `Timer::ScheduleAfter`
    template <typename Duration, typename F, typename... Args>
    TimerId ScheduleAfter(const Duration& , F&& , Args&&...);

    // thread-safe
    // Internal use for future
    void ScheduleLater(std::chrono::milliseconds , std::function<void ()> ) override;
    void Schedule(std::function<void ()> ) override;

    // thread-safe
    // F return non-void
    //
    // Usage:
    //
    // loop.Execute(my_work_func, some_args)
    //     .Then(process_work_result);
    //
    // my_work_func will be executed ASAP, but you SHOULD assure that
    // it will NOT block, otherwise EventLoop will be hang!
    template <typename F, typename... Args,
              typename = typename std::enable_if<!std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type,
              typename Dummy = void>
    auto Execute(F&& , Args&&...) -> Future<typename std::result_of<F (Args...)>::type>;

    // F return void
    template <typename F, typename... Args,
              typename = typename std::enable_if<std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type>
    auto Execute(F&& , Args&&...) -> Future<void>;

    void Run();

    bool Register(int events, std::shared_ptr<internal::Channel> src);
    bool Modify(int events, std::shared_ptr<internal::Channel> src);
    void Unregister(int events, std::shared_ptr<internal::Channel> src);

    std::size_t Size() const {
        return channelSet_.size();
    }

    // check if current thread is same as this loop's thread
    bool InThisLoop() const;

    int Id() const {
        return id_;
    }

    static EventLoop* Self();

    static void SetMaxOpenFd(rlim_t maxfdPlus1);

private:
    bool _Loop(DurationMs timeout);

    internal::EventLoopGroup* group_;
    std::unique_ptr<internal::Poller> poller_;

    std::shared_ptr<internal::PipeChannel> notifier_;

    internal::TimerManager timers_;

    // channelSet_ must be destructed before timers_
    std::map<unsigned int, std::shared_ptr<internal::Channel> > channelSet_;

    std::mutex fctrMutex_;
    std::vector<std::function<void ()> > functors_;

    int id_;
    static std::atomic<int> s_evId;

    static thread_local unsigned int s_id;

    // max open fd + 1
    static rlim_t s_maxOpenFdPlus1;
};


template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId EventLoop::ScheduleAtWithRepeat(const TimePoint& triggerTime,
                                        const Duration& period,
                                        F&& f, Args&&... args) {
    assert (InThisLoop());
    return timers_.ScheduleAtWithRepeat<RepeatCount>(triggerTime,
                                                     period,
                                                     std::forward<F>(f),
                                                     std::forward<Args>(args)...);
}

template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId EventLoop::ScheduleAfterWithRepeat(const Duration& period,
                                           F&& f, Args&&... args) {
    assert (InThisLoop());
    return timers_.ScheduleAfterWithRepeat<RepeatCount>(period,
                                                        std::forward<F>(f),
                                                        std::forward<Args>(args)...);
}

template <typename F, typename... Args>
TimerId EventLoop::ScheduleAt(const TimePoint& triggerTime,
                              F&& f, Args&&... args) {
    assert (InThisLoop());
    return timers_.ScheduleAt(triggerTime,
                              std::forward<F>(f),
                              std::forward<Args>(args)...);
}

template <typename Duration, typename F, typename... Args>
TimerId EventLoop::ScheduleAfter(const Duration& duration,
                                 F&& f, Args&&... args) {
    assert (InThisLoop());
    return timers_.ScheduleAfter(duration,
                                 std::forward<F>(f),
                                 std::forward<Args>(args)...);
}

// If F return something not void, or return Future
template <typename F, typename... Args, typename, typename >

Future<typename std::result_of<F (Args...)>::type>
EventLoop::Execute(F&& f, Args&&... args) {

    using resultType = typename std::result_of<F (Args...)>::type;

    Promise<resultType> promise;
    auto future = promise.GetFuture();

    if (InThisLoop()) {
        promise.SetValue(std::forward<F>(f)(std::forward<Args>(args)...));
    } else {
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto func = [t = std::move(task), pm = std::move(promise)]() mutable {
            try {
                pm.SetValue(Try<resultType>(t()));
            } catch(...) {
                pm.SetException(std::current_exception());
            }
        };

        {
            std::unique_lock<std::mutex> guard(fctrMutex_);
            functors_.emplace_back(std::move(func));
        }

        notifier_->Notify();
    }

    return future;
}

// F return void
template <typename F, typename... Args, typename >
Future<void> EventLoop::Execute(F&& f, Args&&... args) {

    using resultType = typename std::result_of<F (Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    Promise<void> promise;
    auto future = promise.GetFuture();

    if (InThisLoop()) {
        std::forward<F>(f)(std::forward<Args>(args)...);
        promise.SetValue();
    } else {
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto func = [t = std::move(task), pm = std::move(promise)]() mutable {
            try {
                t();
                pm.SetValue();
            } catch(...) {
                pm.SetException(std::current_exception());
            }
        };

        {
            std::unique_lock<std::mutex> guard(fctrMutex_);
            functors_.emplace_back(std::move(func));
        }

        notifier_->Notify();
    }

    return future;
}


} // end namespace ananas

#endif

