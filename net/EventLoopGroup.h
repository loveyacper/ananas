#ifndef BERT_EVENTLOOPGROUP_H
#define BERT_EVENTLOOPGROUP_H

#include <atomic>
#include <vector>

#include "ananas/util/ThreadPool.h"

namespace ananas {

class EventLoop;

namespace internal {

class EventLoopGroup {
public:
    explicit
    EventLoopGroup(size_t nLoops = 1);
    ~EventLoopGroup();

    EventLoopGroup(const EventLoopGroup& ) = delete;
    void operator=(const EventLoopGroup& ) = delete;

    EventLoopGroup(EventLoopGroup&& ) = delete;
    void operator=(EventLoopGroup&& ) = delete;

    void SetNumOfEventLoop(size_t n);
    size_t Size() const;

    void Stop();
    bool IsStopped() const;

    void Start();
    void Wait();

    EventLoop* Next() const;

private:
    enum State {
        eS_None,
        eS_Started,
        eS_Stopped,
    };
    std::atomic<State> state_{eS_None};

    ThreadPool pool_;

    std::mutex mutex_;
    std::condition_variable cond_;
    // Use raw-pointer because unique_ptr needs complete type
    std::vector<EventLoop* > loops_;

    size_t numLoop_;
    mutable std::atomic<size_t> currentLoop_ {0};
};

} // end namespace internal

} // end namespace ananas

#endif

