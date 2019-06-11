#ifndef BERT_THREADPOOL_H
#define BERT_THREADPOOL_H

#include <deque>
#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "ananas/future/Future.h"

///@file ThreadPool.h
///@brief A powerful ThreadPool implementation with Future interface.
///Usage:
///@code
/// pool.Execute(your_heavy_work, some_args)
///     .Then(process_heavy_work_result)
///@endcode
///
///  Here, your_heavy_work will be executed in a thread, and return Future
///  immediately. When it done, function process_heavy_work_result will be called.
///  The type of argument of process_heavy_work_result is the same as the return
///  type of your_heavy_work.
namespace ananas {

///@brief A powerful ThreadPool implementation with Future interface.
class ThreadPool final {
public:
    ThreadPool();
    ~ThreadPool();

    ThreadPool(const ThreadPool& ) = delete;
    void operator=(const ThreadPool& ) = delete;

    ///@brief Execute work in this pool
    ///@return A future, you can register callback on it
    ///when f is done or timeout.
    ///
    /// If the threads size not reach limit, or there are
    /// some idle threads, f will be executed at once.
    /// But if all threads are busy and threads size reach
    /// limit, f will be queueing, will be executed later.
    ///
    /// F returns non-void
    template <typename F, typename... Args,
              typename = typename std::enable_if<!std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type,
              typename Dummy = void>
    auto Execute(F&& f, Args&&... args) -> Future<typename std::result_of<F (Args...)>::type>;

    ///@brief Execute work in this pool
    ///@return A future, you can register callback on it
    ///when f is done or timeout.
    ///
    /// If the threads size not reach limit, or there are
    /// some idle threads, f will be executed at once.
    /// But if all threads are busy and threads size reach
    /// limit, f will be queueing, will be executed later.
    ///
    /// F returns void
    template <typename F, typename... Args,
              typename = typename std::enable_if<std::is_void<typename std::result_of<F (Args...)>::type>::value, void>::type>
    auto Execute(F&& f, Args&&... args) -> Future<void>;

    ///@brief Stop thread pool and wait all threads terminate
    void JoinAll();
    ///@brief Set max size of idle threads
    ///
    /// Details about threads in pool:
    /// Busy threads, they are doing work on behalf of us.
    /// Idle threads, they are waiting on a queue for new work.
    /// Monitor thread, the internal thread, only one, it'll check
    /// the size of idle threads periodly, if there are too many idle
    /// threads, they will be recycled by monitor thread.
    void SetMaxIdleThreads(unsigned int );
    ///@brief Set max size of idle threads
    ///
    /// Max threads size is the total of idle threads and busy threads,
    /// not include monitor thread.
    /// Default value is 1024
    /// Example: if you SetMaxThreads(8), SetMaxIdleThreads(2)
    /// and now execute 8 long working, there will be 8 busy threads,
    /// 0 idle thread, when all work done, will be 0 busy thread, 2 idle
    /// threads, other 6 threads are recycled by monitor thread.
    void SetMaxThreads(unsigned int );

private:
    void _SpawnWorker();
    void _WorkerRoutine();
    void _MonitorRoutine();

    // Recycle redundant threads
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

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task = [t = std::move(func), pm = std::move(promise)]() mutable {
        try {
            pm.SetValue(Try<resultType>(t()));
        } catch(...) {
            pm.SetException(std::current_exception());
        }
    };

    tasks_.emplace_back(std::move(task));
    if (waiters_ == 0 && currentThreads_ < maxThreads_)
        _SpawnWorker();

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

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task = [t = std::move(func), pm = std::move(promise)]() mutable {
        try {
            t();
            pm.SetValue();
        } catch(...) {
            pm.SetException(std::current_exception());
        }
    };

    tasks_.emplace_back(std::move(task));
    if (waiters_ == 0 && currentThreads_ < maxThreads_)
        _SpawnWorker();

    cond_.notify_one();

    return future;
}

} // end namespace ananas

#endif

