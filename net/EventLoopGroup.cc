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

size_t EventLoopGroup::Size() const
{
    return numLoop_;
}
    
void EventLoopGroup::Stop()
{
    state_ = eS_Stopped;
}

bool EventLoopGroup::IsStopped() const 
{
    return state_ == eS_Stopped;
}

void EventLoopGroup::Start()
{
    pool_.SetMaxThreads(numLoop_);
    for (size_t i = 0; i < numLoop_; ++ i)
    {
        pool_.Execute([this]()
                      {
                          EventLoop loop(this);
            
                          {
                              std::unique_lock<std::mutex> guard(mutex_);
                              loops_.push_back(&loop);
                              if (loops_.size() == numLoop_)
                                  cond_.notify_one();
                          }

                          loop.Run();
                      });
    }

    std::unique_lock<std::mutex> guard(mutex_);
    cond_.wait(guard, [this] () { return loops_.size() == numLoop_; });

    state_ = eS_Started;
}

void EventLoopGroup::Wait()
{
    pool_.JoinAll();
}

EventLoop* EventLoopGroup::Next() const
{
    if (state_ != eS_Started)
        return nullptr;

    if (loops_.empty())
        return nullptr;

    if (currentLoop_  >= loops_.size())
        currentLoop_ = 0;

    return loops_[currentLoop_++];
}

} // end namespace internal

} // end namespace ananas

