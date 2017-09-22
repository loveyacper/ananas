#include "ThreadPool.h"

namespace ananas
{

thread_local bool ThreadPool::working_ = true;
std::thread::id ThreadPool::s_mainThread;

ThreadPool::ThreadPool() : waiters_(0), shutdown_(false)
{
    monitor_ = std::thread([this]() { _MonitorRoutine(); } );
    maxIdleThread_ = std::max(1U, std::thread::hardware_concurrency());
    pendingStopSignal_ = 0;

    // init main thread id 
    s_mainThread = std::this_thread::get_id();
}

ThreadPool::~ThreadPool()
{
    JoinAll();
}

ThreadPool& ThreadPool::Instance()
{
    static ThreadPool  pool;
    return pool;
}

void ThreadPool::SetMaxIdleThread(unsigned int m)
{
    if (0 < m && m <= kMaxThreads)
        maxIdleThread_ = m;
}

void ThreadPool::JoinAll()
{
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
    
    for (auto& t : tmp)
    {
        if (t.joinable())
            t.join();
    }
    
    if (monitor_.joinable())
    {
        monitor_.join();
    }
}

void ThreadPool::_CreateWorker()
{
    std::thread t([this]() { this->_WorkerRoutine(); } );
    workers_.push_back(std::move(t));
}

void ThreadPool::_WorkerRoutine()
{
    working_ = true;
    
    while (working_)
    {
        std::function<void ()>   task;
        
        {
            std::unique_lock<std::mutex>    guard(mutex_);
            
            ++ waiters_;
            cond_.wait(guard, [this]() { return shutdown_ || !tasks_.empty(); } );
            -- waiters_;
            
            if (shutdown_ && tasks_.empty())
                return;
            
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        
        task();
    }
    
    // if reach here, this thread is recycled by monitor thread
    -- pendingStopSignal_;
}

void ThreadPool::_MonitorRoutine()
{
    while (!shutdown_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::unique_lock<std::mutex> guard(mutex_);
        if (shutdown_)
            return;

        auto nw = waiters_;

        // if there is any pending stop signal to consume waiters
        nw -= pendingStopSignal_;

        while (nw -- > maxIdleThread_)
        {
            tasks_.push_back([this]() { working_ = false; });
            cond_.notify_one();
            ++ pendingStopSignal_;
        }
    }
}

} // end namespace ananas

