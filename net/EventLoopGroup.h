#ifndef BERT_EVENTLOOPGROUP_H
#define BERT_EVENTLOOPGROUP_H

#include <memory>
#include <vector>

#include "ThreadPool.h"
//#include "Poller.h"
//#include "Timer.h"

namespace ananas
{

class EventLoop;

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

} // end namespace ananas

#endif

