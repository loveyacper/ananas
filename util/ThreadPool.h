#ifndef BERT_THREADPOOL_H
#define BERT_THREADPOOL_H

#include <deque>
#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "ananas/future/Future.h"

namespace ananas {

class ThreadPool final {
public:
    ThreadPool();
    ~ThreadPool();

    ThreadPool(const ThreadPool& ) = delete;
    void operator=(const ThreadPool& ) = delete;

    // F return non-void
    template <typename F, typename... Args,
              typename = typename std::enable_if<!std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type, typename Dummy = void>
    auto Execute(F&& f, Args&&... args) -> Future<typename std::result_of<F (Args...)>::type>;

    // F return void
    template <typename F, typename... Args,
              typename = typename std::enable_if<std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type>
    auto Execute(F&& f, Args&&... args) -> Future<void>;

    void JoinAll();
    void SetMaxIdleThreads(unsigned int );
    void SetMaxThreads(unsigned int );

private:
    void _SpawnWorker();
    void _WorkerRoutine();
    void _MonitorRoutine();

    std::thread monitor_;
    std::atomic<unsigned> maxThreads_;
    std::atomic<unsigned> currentThreads_;
    std::atomic<unsigned> maxIdleThreads_;
    std::atomic<unsigned> pendingStopSignal_;

    static thread_local bool working_;
    std::deque<std::thread> workers_;

    std::mutex mutex_;
    std::condition_variable cond_;
    unsigned waiters_;
    bool shutdown_;
    std::deque<std::function<void ()> > tasks_;

    static const int kMaxThreads = 1024;
    static std::thread::id s_mainThread;
};


// if F return something
template <typename F, typename... Args, typename, typename >
auto ThreadPool::Execute(F&& f, Args&&... args) -> Future<typename std::result_of<F (Args...)>::type> {
    using resultType = typename std::result_of<F (Args...)>::type;

    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_)
        return MakeReadyFuture<resultType>(resultType());

    Promise<resultType> promise;
    auto future = promise.GetFuture();

    auto innerTask = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task = [t = std::move(innerTask), promise = std::move(promise)]() mutable {
        try {
            promise.SetValue(Try<resultType>(t()));
        } catch(...) {
            promise.SetException(std::current_exception());
        }
    };

    tasks_.emplace_back(std::move(task));
    if (waiters_ == 0 && currentThreads_ < maxThreads_)
        _SpawnWorker();

    guard.unlock();
    cond_.notify_one();

    return future;
}

// F return void
template <typename F, typename... Args, typename >
auto ThreadPool::Execute(F&& f, Args&&... args) -> Future<void> {
    using resultType = typename std::result_of<F (Args...)>::type;
    static_assert(std::is_void<resultType>::value, "must be void");

    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_)
        return MakeReadyFuture();

    Promise<resultType> promise;
    auto future = promise.GetFuture();

    auto innerTask = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task = [t = std::move(innerTask), promise = std::move(promise)]() mutable {
        try {
            t();
            promise.SetValue();
        } catch(...) {
            promise.SetException(std::current_exception());
        }
    };

    tasks_.emplace_back(std::move(task));
    if (waiters_ == 0 && currentThreads_ < maxThreads_)
        _SpawnWorker();

    guard.unlock();
    cond_.notify_one();

    return future;
}

} // end namespace ananas

#endif

