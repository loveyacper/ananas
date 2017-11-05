
#ifndef BERT_TIMERMANAGER_H
#define BERT_TIMERMANAGER_H

#include <map>
#include <chrono>
#include <functional>
#include <memory>
#include <ostream>

namespace ananas
{

using DurationMs = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;
using TimerId = std::shared_ptr<std::pair<TimePoint, unsigned int> >;

constexpr int kForever = -1;

inline std::ostream& operator<< (std::ostream& os, const TimerId& d)
{
    os << "[TimerId:" << (void*)d.get() << "]";
    return os;
}

namespace internal
{

class TimerManager final
{
public:
    TimerManager();
   ~TimerManager();

    TimerManager(const TimerManager& ) = delete;
    void operator= (const TimerManager& ) = delete;

    // Tick
    void Update();

    // Schedule timer at absolute timepoint
    template <int RepeatCount = 1/* -1 is forever */, typename F, typename... Args>
    TimerId ScheduleAt(const TimePoint& triggerTime, F&& f, Args&&... args); // TODO ScheduleAt

    // Schedule timer after some time duration
    template <int RepeatCount = 1, typename Duration, typename F, typename... Args>
    TimerId ScheduleAfter(const Duration& duration, F&& f, Args&&... args);

    // Cancel timer
    bool Cancel(TimerId id);

    // how far the nearest timer will be trigger.
    DurationMs NearestTimer() const;

private:
    class Timer
    {
        friend class TimerManager;
    public:
        explicit
        Timer(const TimePoint& tp);

        // only move
        Timer(Timer&& timer);
        Timer& operator= (Timer&& );

        Timer(const Timer& ) = delete;
        void operator= (const Timer& ) = delete;

        template <typename F, typename... Args>
        void SetCallback(F&& f, Args&&... args);

        void OnTimer();

        TimerId Id() const;
        unsigned int UniqueId() const;

    private:
        void _Move(Timer&& timer);

        TimerId id_;

        std::function<void ()> func_;
        DurationMs interval_;
        int count_;
    };

    std::multimap<TimePoint, Timer> timers_;

    TimePoint now_;

    friend class Timer;
    // not thread-safe, but who cares?
    static unsigned int s_timerIdGen_;
};


template <int RepeatCount, typename F, typename... Args>
TimerId TimerManager::ScheduleAt(const TimePoint& triggerTime, F&& f, Args&&... args)
{
    static_assert(RepeatCount != 0, "Why you add a timer with zero count?");

    using namespace std::chrono;

    Timer t(triggerTime);
    // precision: milliseconds
    t.interval_ = std::max(DurationMs(1), duration_cast<DurationMs>(triggerTime - now_));
    t.count_ = RepeatCount;
    TimerId id = t.Id();

    t.SetCallback(std::forward<F>(f), std::forward<Args>(args)...);
    timers_.insert(std::make_pair(triggerTime, std::move(t)));
    return id;
}

template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId TimerManager::ScheduleAfter(const Duration& duration, F&& f, Args&&... args)
{
    this->now_ = std::chrono::steady_clock::now();
    return ScheduleAt<RepeatCount>(now_ + duration,
                                   std::forward<F>(f),
                                   std::forward<Args>(args)...);
}

template <typename F, typename... Args>
void TimerManager::Timer::SetCallback(F&& f, Args&&... args)
{
    auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    func_ = [temp]() mutable { (void)temp(); };
}

} // end namespace internal
} // end namespace ananas

#endif

