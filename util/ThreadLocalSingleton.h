#ifndef BERT_THREADLOCALSINGLETON_H
#define BERT_THREADLOCALSINGLETON_H

#include <mutex>
#include <thread>
#include <map>
#include <stdexcept>
#include <cstdlib>

namespace ananas
{

template <typename T>
class ThreadLocalSingleton
{
    friend T;

private:
    ThreadLocalSingleton()
    {
    }
    
public:
    // the public interface
    static T& ThreadInstance() noexcept
    {
        static std::once_flag s_init;

        std::call_once(s_init, [&]() {
            mutex_ = new std::mutex();
            instances_ = new std::map<std::thread::id, T* >();

            std::atexit(_Destroy);
        });

        if (!inst_)
        {
            auto tid = std::this_thread::get_id();

            std::unique_lock<std::mutex> guard(*mutex_);
            (*instances_)[tid] = (inst_ = new T());
        }

        return *inst_;
    }

    // no copyable
    ThreadLocalSingleton(const ThreadLocalSingleton& ) = delete;
    void operator=(const ThreadLocalSingleton& ) = delete;

    // no movable
    ThreadLocalSingleton(ThreadLocalSingleton&& ) = delete;
    void operator=(ThreadLocalSingleton&& ) = delete;
    
private:
    static void _Destroy()
    {
        std::unique_lock<std::mutex> guard(*mutex_);
        for (const auto& kv : *instances_)
            delete kv.second;

        delete mutex_;
        delete instances_;
    }

    static std::mutex* mutex_;
    static std::map<std::thread::id, T* >* instances_;

    static thread_local T* inst_;
};


template <typename T>
std::mutex* ThreadLocalSingleton<T>::mutex_ = nullptr;

template <typename T>
std::map<std::thread::id, T*>* ThreadLocalSingleton<T>::instances_ = nullptr;

template <typename T>
thread_local T* ThreadLocalSingleton<T>::inst_ = nullptr;

#define DECLARE_THREAD_SINGLETON(type)  friend class ThreadLocalSingleton<type>

} // end namespace ananas

#endif

