
#ifndef BERT_TIMEUTIL_H
#define BERT_TIMEUTIL_H

#include <ctime>
#include <sys/time.h>
#include <stdint.h>
#include <memory>
#include <mutex>
#include <atomic>

uint64_t Now();

class Time
{
public:
    Time();
    Time(const Time& other);

    void Now();
    uint64_t MilliSeconds() const { return ms_; }
    std::size_t FormatTime(char* buf) const;

    int GetYear()   const { _UpdateTm(); return tm_.tm_year + 1900;  } 
    int GetMonth()  const { _UpdateTm(); return tm_.tm_mon + 1;  } 
    int GetDay()    const { _UpdateTm(); return tm_.tm_mday; }
    int GetHour()   const { _UpdateTm(); return tm_.tm_hour; }
    int GetMinute() const { _UpdateTm(); return tm_.tm_min;  }
    int GetSecond() const { _UpdateTm(); return tm_.tm_sec;  }

    Time&  operator= (const Time& );
    operator uint64_t() const { return ms_; }

private:
    uint64_t ms_;      // milliseconds from 1970
    mutable tm tm_;
    mutable bool valid_;
    
    void _UpdateTm() const;
};


#endif

