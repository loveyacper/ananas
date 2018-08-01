#include <cassert>
#include "ThreadPool.h"

namespace ananas {

thread_local bool ThreadPool::working_ = true;
std::thread::id ThreadPool::s_mainThread;

ThreadPool::ThreadPool() : currentThreads_{0}, waiters_(0), shutdown_(false) {
    maxIdleThreads_ = std::max(1U, std::thread::hardware_concurrency());
    maxThreads_ = kMaxThreads;
    pendingStopSignal_ = 0;

    // init main thread id
    s_mainThread = std::this_thread::get_id();

    monitor_ = std::thread([this]() {
        _MonitorRoutine();
    } );
}

ThreadPool::~ThreadPool() {
    JoinAll();
}

void ThreadPool::SetMaxIdleThreads(unsigned int m) {
    if (0 < m && m <= kMaxThreads)
        maxIdleThreads_ = m;
}

void ThreadPool::SetMaxThreads(unsigned int m) {
    if (0 < m && m <= kMaxThreads)
        maxThreads_ = m;
}

void ThreadPool::JoinAll() {
    if (s_mainThread != std::this_thread::get_id())
        return;

    decltype(workers_)  tmp;

    {
        std::unique_lock<std::mutex>  guard(mutex_);
        if (shutdown_)
            return;

        shutdown_ = true;
        cond_.notify_all();

        tmp.swap(workers_);
    }

    for (auto& t : tmp) {
        if (t.joinable())
            t.join();
    }

    if (monitor_.joinable()) {
        monitor_.join();
    }
}

void ThreadPool::_SpawnWorker() {
    // guarded by mutex.
    ++ currentThreads_;
    std::thread t([this]() {
        this->_WorkerRoutine();
    } );
    workers_.push_back(std::move(t));
}

void ThreadPool::_WorkerRoutine() {
    working_ = true;

    while (working_) {
        std::function<void ()> task;

        {
            std::unique_lock<std::mutex> guard(mutex_);

            ++ waiters_;
            cond_.wait(guard, [this]() {
                return shutdown_ || !tasks_.empty();
            } );
            -- waiters_;

            assert(shutdown_ || !tasks_.empty());
            if (shutdown_ && tasks_.empty()) {
                -- currentThreads_;
                return;
            }

            assert (!tasks_.empty());
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        task();
    }

    // if reach here, this thread is recycled by monitor thread
    -- currentThreads_;
    -- pendingStopSignal_;
}

void ThreadPool::_MonitorRoutine() {
    while (!shutdown_) {
        // Do NOT use mutex and cond, otherwise `Execute` will awake me
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        std::unique_lock<std::mutex> guard(mutex_);
        if (shutdown_)
            return;

        auto nw = waiters_;

        // if there is any pending stop signal to consume waiters
        nw -= pendingStopSignal_;

        while (nw -- > maxIdleThreads_) {
            tasks_.push_back([this]() {
                working_ = false;
            });
            cond_.notify_one();
            ++ pendingStopSignal_;
        }
    }
}

} // end namespace ananas

