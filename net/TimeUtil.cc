#include <cstring>
#include "TimeUtil.h"

namespace ananas
{

Time::Time() : valid_(false)
{
    this->Now();
}

int64_t Time::MilliSeconds() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(ms_.time_since_epoch()).count();
}

void Time::Now()
{
    ms_ = std::chrono::system_clock::now();
    valid_ = false;
}

void Time::_UpdateTm()  const
{
    // lazy compute
    if (valid_)  
        return;

    valid_ = true; 
    const time_t now(MilliSeconds() / 1000UL);
    ::localtime_r(&now, &tm_);
}

std::once_flag Time::init_;

// from 2015 to 2029
const char* Time::YEAR[] = { "2015", "2016", "2017", "2018", "2019", "2020",
    "2021", "2022", "2023", "2024", "2025", "2026", "2027", "2028", "2029",
     nullptr,
};

char Time::NUMBER[60][2] = {""};

void Time::Init() 
{
    for (size_t i = 0; i < sizeof NUMBER / sizeof NUMBER[0]; ++ i)
    {
        char tmp[3]; 
        snprintf(tmp, 3, "%02d", static_cast<int>(i));
        memcpy(NUMBER[i], tmp, 2);
    }
}

std::size_t Time::FormatTime(char* buf) const
{
    std::call_once(init_, &Time::Init);

    _UpdateTm();
    
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
    auto msec = MilliSeconds();
    snprintf(buf + 20, 5, "%03d]", static_cast<int>(msec % 1000));
    
    return 24;
}

} // end namespace ananas

