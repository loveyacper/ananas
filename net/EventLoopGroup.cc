#include "EventLoopGroup.h"
#include "EventLoop.h"

namespace ananas
{

namespace internal
{

EventLoopGroup::EventLoopGroup(size_t nLoops)
{
    numLoop_ = nLoops;
}

EventLoopGroup::~EventLoopGroup()
{
}

void EventLoopGroup::SetNumOfEventLoop(size_t n)
{
    assert (n <= 1024);
    numLoop_ = n;
}
    
void EventLoopGroup::Stop()
{
    stop_ = true;
}

bool EventLoopGroup::IsStopped() const 
{
    return stop_;
}

void EventLoopGroup::Start()
{
    pool_.SetMaxThreads(numLoop_);
    for (size_t i = 0; i < numLoop_; ++ i)
    {
        pool_.Execute([this]()
                      {
                          EventLoop loop(this);
                          loops_.push_back(&loop);
                          loop.Run();
                      });
    }
}

void EventLoopGroup::Wait()
{
    pool_.JoinAll();
}

EventLoop* EventLoopGroup::Next() const
{
    if (loops_.empty())
        return nullptr;

    if (currentLoop_  >= loops_.size())
        currentLoop_ = 0;

    return loops_[currentLoop_++];
}

} // end namespace internal

} // end namespace ananas

