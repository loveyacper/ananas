#ifndef BERT_THREADPOOL_H
#define BERT_THREADPOOL_H

#include <deque>
#include <thread>
#include <memory>
#include <future>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace ananas
{

class ThreadPool final
{
public:
    ~ThreadPool();
    
    ThreadPool(const ThreadPool& ) = delete;
    void operator=(const ThreadPool& ) = delete;
    
    static ThreadPool& Instance();
    
    template <typename F, typename... Args>
    auto Execute(F&& f, Args&&... args) -> std::future<typename std::result_of<F (Args...)>::type>;
    
    void JoinAll();
    void SetMaxIdleThread(unsigned int m);
    
private:
    ThreadPool();
    
    void _CreateWorker();
    void _WorkerRoutine();
    void _MonitorRoutine();
    
    std::thread monitor_;
    std::atomic<unsigned> maxIdleThread_;
    std::atomic<unsigned> pendingStopSignal_;
    
    static thread_local bool working_;
    std::deque<std::thread> workers_;
    
    std::mutex mutex_;
    std::condition_variable cond_;
    unsigned waiters_;
    bool shutdown_;
    std::deque<std::function<void ()> > tasks_;
    
    static const int kMaxThreads = 256;
};


template <typename F, typename... Args>
auto ThreadPool::Execute(F&& f, Args&&... args) -> std::future<typename std::result_of<F (Args...)>::type>
{
    using resultType = typename std::result_of<F (Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<resultType ()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    
    {
        std::unique_lock<std::mutex> guard(mutex_);
        if (shutdown_)
            return std::future<resultType>();
        
        tasks_.emplace_back( [=]() { (*task)(); } );
        if (waiters_ == 0)
            _CreateWorker();
        
        cond_.notify_one();
    }
    
    return task->get_future();
}

} // end namespace ananas

#endif

