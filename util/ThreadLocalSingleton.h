#ifndef BERT_THREADLOCALSINGLETON_H
#define BERT_THREADLOCALSINGLETON_H

#include <mutex>
#include <thread>
#include <map>
#include <stdexcept>

namespace ananas
{

template <typename T>
class ThreadLocalSingleton
{
public:
    ThreadLocalSingleton()
    {
        std::unique_lock<std::mutex> guard(thisMutex_);
        if (this_)
            throw std::runtime_error("Don't declare multi ThreadLocalSingleton objects with the same inner type");
        else
            this_ = this;
    }
    
    // the public interface
    static T& Instance() noexcept
    {
        if (!inst_)
        {
            auto tid = std::this_thread::get_id();

            std::unique_lock<std::mutex> guard(this_->mutex_);
            this_->instances_[tid] = (inst_ = new T());
        }

        return *inst_;
    }

    ~ThreadLocalSingleton() noexcept
    {
        decltype(instances_) tmp;

        {
            std::unique_lock<std::mutex> guard(mutex_);
            tmp.swap(instances_);
        }

        for (const auto& kv : tmp)
            delete kv.second;
    }

    // no copyable
    ThreadLocalSingleton(const ThreadLocalSingleton& ) = delete;
    void operator=(const ThreadLocalSingleton& ) = delete;

    // no movable
    ThreadLocalSingleton(ThreadLocalSingleton&& ) = delete;
    void operator=(ThreadLocalSingleton&& ) = delete;
    
private:
    std::mutex mutex_;
    std::map<std::thread::id, T* > instances_;

    static std::mutex thisMutex_;
    static ThreadLocalSingleton<T>* this_;
    static thread_local T* inst_;
};


template <typename T>
std::mutex ThreadLocalSingleton<T>::thisMutex_;

template <typename T>
ThreadLocalSingleton<T>* ThreadLocalSingleton<T>::this_ = nullptr;

template <typename T>
thread_local T* ThreadLocalSingleton<T>::inst_ = nullptr;

} // end namespace ananas

#endif

