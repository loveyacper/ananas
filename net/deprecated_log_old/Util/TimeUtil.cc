#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include "TimeUtil.h"

uint64_t Now()
{
    struct timeval now;
    ::gettimeofday(&now, 0);
    return  uint64_t(now.tv_sec * 1000UL + now.tv_usec / 1000UL);
}

Time::Time() : ms_(0), valid_(false)
{
    tm_.tm_year = 0;
    this->Now();
}

Time::Time(const Time& other) : ms_(other.ms_)
{
    tm_.tm_year = 0;
    valid_ = false;
}

void Time::_UpdateTm()  const
{
    if (valid_)  
        return;

    valid_ = true; 
    const time_t now(ms_ / 1000UL); 
    ::localtime_r(&now, &tm_);
}

void Time::Now()
{
    ms_ = ::Now();
    valid_ = false;
}

// from 2015 to 2025
static const char* YEAR[] = { "2015", "2016", "2017", "2018",
    "2019", "2020", "2021", "2022", "2023", "2024", "2025",
     nullptr,
};

std::size_t Time::FormatTime(char* buf) const
{
    static char NUMBER[60][2] = {""};

    static bool bFirst = true;
    if (bFirst)
    {
        bFirst = false;
        for (size_t i = 0; i < sizeof NUMBER / sizeof NUMBER[0]; ++ i)
        {
            char tmp[3]; 
            snprintf(tmp, 3, "%02d", static_cast<int>(i));
            memcpy(NUMBER[i], tmp, 2);
        }
    }

    _UpdateTm();
    
#if  1
    memcpy(buf, YEAR[tm_.tm_year + 1900 - 2015], 4);
    buf[4] = '-';
    memcpy(buf + 5, NUMBER[tm_.tm_mon + 1], 2);
    buf[7] = '-';
    memcpy(buf + 8, NUMBER[tm_.tm_mday], 2);
    buf[10] = '[';
    memcpy(buf + 11, NUMBER[tm_.tm_hour], 2);
    buf[13] = ':';
    memcpy(buf + 14, NUMBER[tm_.tm_min], 2);
    buf[16] = ':';
    memcpy(buf + 17, NUMBER[tm_.tm_sec], 2);
    buf[19] = '.';
    snprintf(buf + 20, 5, "%03d]", static_cast<int>(ms_ % 1000));
#else
    
    snprintf(buf, 25, "%04d-%02d-%02d[%02d:%02d:%02d.%03d]",
             tm_.tm_year+1900, tm_.tm_mon+1, tm_.tm_mday,
             tm_.tm_hour, tm_.tm_min, tm_.tm_sec,
             static_cast<int>(ms_ % 1000));
#endif
    
    return 24;
}

Time& Time::operator= (const Time & other)
{
    if (this != &other)
    {
        ms_    = other.ms_;
        valid_ = false;
    }
    return *this;
}

