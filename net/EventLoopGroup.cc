#include "EventLoopGroup.h"
#include "EventLoop.h"

namespace ananas {

namespace internal {

EventLoopGroup::EventLoopGroup(size_t nLoops) {
    numLoop_ = nLoops;
}

EventLoopGroup::~EventLoopGroup() {
    this->Wait();
}

void EventLoopGroup::SetNumOfEventLoop(size_t n) {
    assert (n <= 1024);
    numLoop_ = n;
}

size_t EventLoopGroup::Size() const {
    return numLoop_;
}

void EventLoopGroup::Stop() {
    state_ = eS_Stopped;
}

bool EventLoopGroup::IsStopped() const {
    return state_ == eS_Stopped;
}

void EventLoopGroup::Start() {
    // only called by main thread
    assert (state_ == eS_None);

    pool_.SetNumOfThreads(numLoop_);
    for (size_t i = 0; i < numLoop_; ++i) {
        pool_.Execute([this]() {
            EventLoop* loop = new EventLoop(this);

            {
                std::unique_lock<std::mutex> guard(mutex_);
                loops_.push_back(loop);
                if (loops_.size() == numLoop_)
                    cond_.notify_one();
            }

            loop->Run();
        });
    }

    std::unique_lock<std::mutex> guard(mutex_);
    cond_.wait(guard, [this] () {
        return loops_.size() == numLoop_;
    });

    state_ = eS_Started;
}

void EventLoopGroup::Wait() {
    pool_.JoinAll();

    for (auto loop : loops_)
        delete loop;

    loops_.clear();
}

EventLoop* EventLoopGroup::Next() const {
    if (state_ != eS_Started)
        return nullptr;

    if (loops_.empty())
        return nullptr;

    return loops_[currentLoop_++ % loops_.size()];
}

} // end namespace internal

} // end namespace ananas

