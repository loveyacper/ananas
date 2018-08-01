
#ifndef BERT_TIMEUTIL_H
#define BERT_TIMEUTIL_H

#include <ctime>
#include <stdint.h>
#include <mutex>
#include <chrono>

namespace ananas {

class Time {
public:
    Time();

    void Now();
    int64_t MilliSeconds() const;
    int64_t MicroSeconds() const;
    std::size_t FormatTime(char* buf) const;

    int GetYear()   const {
        _UpdateTm();
        return tm_.tm_year + 1900;
    }
    int GetMonth()  const {
        _UpdateTm();
        return tm_.tm_mon + 1;
    }
    int GetDay()    const {
        _UpdateTm();
        return tm_.tm_mday;
    }
    int GetHour()   const {
        _UpdateTm();
        return tm_.tm_hour;
    }
    int GetMinute() const {
        _UpdateTm();
        return tm_.tm_min;
    }
    int GetSecond() const {
        _UpdateTm();
        return tm_.tm_sec;
    }

    operator int64_t() const {
        return MilliSeconds();
    }

private:
    std::chrono::system_clock::time_point now_;
    mutable tm tm_;
    mutable bool valid_;

    void _UpdateTm() const;

    // for FormatTime
    static std::once_flag init_;
    static void Init();
    static const char* YEAR[];
    static char NUMBER[60][2];
};

} // end namespace ananas

#endif

