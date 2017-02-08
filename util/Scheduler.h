#ifndef BERT_SCHEDULER_H
#define BERT_SCHEDULER_H

namespace ananas
{

class Scheduler
{
public:
    virtual ~Scheduler() {}
    virtual void ScheduleOnceAfter(std::chrono::milliseconds duration, std::function<void()> f) = 0;
    virtual void ScheduleOnce(std::function<void()> f) = 0;
};

} // end namespace ananas

#endif

