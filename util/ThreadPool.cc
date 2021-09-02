#include <cassert>
#include "ThreadPool.h"

namespace ananas {
std::thread::id ThreadPool::s_mainThread;

ThreadPool::ThreadPool() {
    // init main thread id
    s_mainThread = std::this_thread::get_id();
}

ThreadPool::~ThreadPool() {
    JoinAll();
}

void ThreadPool::SetNumOfThreads(int n) {
    assert(n >= 0 && n <= kMaxThreads);
    numThreads_ = n;
}

void ThreadPool::_Start() {
  if (shutdown_) {
    return;
  }

  assert(workers_.empty());

  for (int i = 0; i < numThreads_; i++) {
    std::thread t([this]() { this->_WorkerRoutine(); });
    workers_.push_back(std::move(t));
  }
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
}

void ThreadPool::_WorkerRoutine() {
    while (true) {
        std::function<void ()> task;

        {
            std::unique_lock<std::mutex> guard(mutex_);

            cond_.wait(guard, [this]() {
                return shutdown_ || !tasks_.empty();
            } );

            assert(shutdown_ || !tasks_.empty());
            if (shutdown_ && tasks_.empty()) {
                return;
            }

            assert (!tasks_.empty());
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        task();
    }
}

size_t ThreadPool::WorkerThreads() const {
  std::unique_lock<std::mutex> guard(mutex_);
  return workers_.size();
}

size_t ThreadPool::Tasks() const {
  std::unique_lock<std::mutex> guard(mutex_);
  return tasks_.size();
}

} // end namespace ananas

