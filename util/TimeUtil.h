
#ifndef BERT_TIMEUTIL_H
#define BERT_TIMEUTIL_H

#include <ctime>
#include <stdint.h>
#include <mutex>
#include <chrono>

///@file TimeUtil.h
namespace ananas {

///@brief Simple encapsulation for time point
class Time {
public:
    Time();

    ///@brief Update this with now
    void Now();
    ///@brief Return milli seconds since 1970
    int64_t MilliSeconds() const;
    ///@brief Return micro seconds since 1970
    int64_t MicroSeconds() const;
    ///@brief Format time with format 1970-01-01[12:00:00.123]
    /// TODO user defined format
    std::size_t FormatTime(char* buf) const;

    ///@brief Get year of this time
    ///@return year like 2017
    int GetYear() const {
        _UpdateTm();
        return tm_.tm_year + 1900;
    }
    ///@brief Get month of this time
    ///@return month like 12
    int GetMonth() const {
        _UpdateTm();
        return tm_.tm_mon + 1;
    }
    ///@brief Get day of this time
    ///@return day like 32
    int GetDay() const {
        _UpdateTm();
        return tm_.tm_mday;
    }
    ///@brief Get hour of this time
    ///@return hour like 23
    int GetHour() const {
        _UpdateTm();
        return tm_.tm_hour;
    }
    ///@brief Get minute of this time
    ///@return minute like 59
    int GetMinute() const {
        _UpdateTm();
        return tm_.tm_min;
    }
    ///@brief Get second of this time
    ///@return second like 59
    int GetSecond() const {
        _UpdateTm();
        return tm_.tm_sec;
    }

    ///@brief Impicitly convert to milliseconds
    ///@code
    /// Time start;
    /// DoSomeThing();
    /// Time end;
    /// printf("used %ld ms\n", end - start);
    ///@endcode
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

