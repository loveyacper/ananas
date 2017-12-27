#ifndef BERT_EVENTLOOPGROUP_H
#define BERT_EVENTLOOPGROUP_H

#include <atomic>
#include <vector>

#include "ThreadPool.h"

namespace ananas
{

class EventLoop;

namespace internal
{

class EventLoopGroup
{
public:
    explicit
    EventLoopGroup(size_t nLoops = 1);
   ~EventLoopGroup();

    EventLoopGroup(const EventLoopGroup& ) = delete;
    void operator=(const EventLoopGroup& ) = delete;

    EventLoopGroup(EventLoopGroup&& ) = delete;
    void operator=(EventLoopGroup&& ) = delete;

    void SetNumOfEventLoop(size_t n);

    void Stop();
    bool IsStopped() const;

    void Start();
    void Wait();

    EventLoop* Next() const;

private:
    std::atomic<bool> stop_ {false};

    ThreadPool pool_;
    std::vector<EventLoop* > loops_;
    size_t numLoop_;
    mutable size_t currentLoop_ {0};
};

} // end namespace internal

} // end namespace ananas

#endif

