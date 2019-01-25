
#ifndef BERT_TIMERMANAGER_H
#define BERT_TIMERMANAGER_H

#include <map>
#include <chrono>
#include <functional>
#include <memory>
#include <ostream>

///@file Timer.h
namespace ananas {

using DurationMs = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;
using TimerId = std::shared_ptr<std::pair<TimePoint, unsigned int> >;

constexpr int kForever = -1;

inline std::ostream& operator<< (std::ostream& os, const TimerId& d) {
    os << "[TimerId:" << (void*)d.get() << "]";
    return os;
}

namespace internal {

///@brief TimerManager class
///
/// You should not used it directly, but via Eventloop
class TimerManager final {
public:
    TimerManager();
    ~TimerManager();

    TimerManager(const TimerManager& ) = delete;
    void operator= (const TimerManager& ) = delete;

    // Tick
    void Update();

    ///@brief Schedule timer at absolute timepoint then repeat with period
    ///@param triggerTime The absolute time when timer first triggered
    ///@param period After first trigger, will be trigger by this period repeated until RepeatCount
    ///@param f The function to execute
    ///@param args Args for f
    ///
    /// RepeatCount: Timer will be canceled after trigger RepeatCount times, kForever implies forever.
    template <int RepeatCount, typename Duration, typename F, typename... Args>
    TimerId ScheduleAtWithRepeat(const TimePoint& triggerTime, const Duration& period, F&& f, Args&&... args);

    ///@brief Schedule timer with period
    ///@param period: Timer will be triggered every period
    ///
    /// RepeatCount: Timer will be canceled after triggered RepeatCount times, kForever implies forever.
    /// PAY ATTENTION: Timer's first trigger isn't at once, but after period time
    template <int RepeatCount, typename Duration, typename F, typename... Args>
    TimerId ScheduleAfterWithRepeat(const Duration& period, F&& f, Args&&... args);

    ///@brief Schedule timer at timepoint
    ///@param triggerTime: The absolute time when timer trigger
    ///
    /// It'll trigger only once
    template <typename F, typename... Args>
    TimerId ScheduleAt(const TimePoint& triggerTime, F&& f, Args&&... args);

    ///@brief Schedule timer after duration
    ///@param duration: After duration, timer will be triggered
    ///
    /// It'll trigger only once
    template <typename Duration, typename F, typename... Args>
    TimerId ScheduleAfter(const Duration& duration, F&& f, Args&&... args);

    ///@brief Cancel timer
    bool Cancel(TimerId id);

    ///@brief how far the nearest timer will be trigger.
    DurationMs NearestTimer() const;

private:
    class Timer {
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

    friend class Timer;

    // not thread-safe, but who cares?
    static unsigned int s_timerIdGen_;
};


template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId TimerManager::ScheduleAtWithRepeat(const TimePoint& triggerTime, const Duration& period, F&& f, Args&&... args) {
    static_assert(RepeatCount != 0, "Why you add a timer with zero count?");

    using namespace std::chrono;

    Timer t(triggerTime);
    // precision: milliseconds
    t.interval_ = std::max(DurationMs(1), duration_cast<DurationMs>(period));
    t.count_ = RepeatCount;
    TimerId id = t.Id();

    t.SetCallback(std::forward<F>(f), std::forward<Args>(args)...);
    timers_.insert(std::make_pair(triggerTime, std::move(t)));
    return id;
}

template <int RepeatCount, typename Duration, typename F, typename... Args>
TimerId TimerManager::ScheduleAfterWithRepeat(const Duration& period, F&& f, Args&&... args) {
    const auto now = std::chrono::steady_clock::now();
    return ScheduleAtWithRepeat<RepeatCount>(now + period,
                                             period,
                                             std::forward<F>(f),
                                             std::forward<Args>(args)...);
}

template <typename F, typename... Args>
TimerId TimerManager::ScheduleAt(const TimePoint& triggerTime, F&& f, Args&&... args) {
    return ScheduleAtWithRepeat<1>(triggerTime,
                                   DurationMs(0), // dummy
                                   std::forward<F>(f),
                                   std::forward<Args>(args)...);
}

template <typename Duration, typename F, typename... Args>
TimerId TimerManager::ScheduleAfter(const Duration& duration, F&& f, Args&&... args) {
    const auto now = std::chrono::steady_clock::now();
    return ScheduleAt(now + duration,
                      std::forward<F>(f),
                      std::forward<Args>(args)...);
}

template <typename F, typename... Args>
void TimerManager::Timer::SetCallback(F&& f, Args&&... args) {
    func_ = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
}

} // end namespace internal
} // end namespace ananas

#endif

