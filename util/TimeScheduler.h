#ifndef BERT_TIMESCHEDULER_H
#define BERT_TIMESCHEDULER_H

namespace ananas
{

class TimeScheduler
{
public:
    virtual ~TimeScheduler() {}
    virtual void ScheduleOnceAfter(std::chrono::milliseconds duration, std::function<void()> f) = 0;
};

} // end namespace ananas

#endif

