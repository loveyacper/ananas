
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "TimeUtil.h"
#include "Logger.h"

namespace ananas
{

namespace
{

enum LogColor
{
    Red = 1,
    Green,
    Yellow,
    Normal,
    Blue,
    Purple,
    White,
    Max,
};

}

// TODO config
static const size_t kDefaultLogSize = 32 * 1024 * 1024;

static const size_t kPrefixLevelLen = 6;
static const size_t kPrefixTimeLen = 27;

static bool MakeDir(const char* dir)
{
    if (mkdir(dir, 0755) != 0)
    {
        if (EEXIST != errno)
        {
            perror("MakeDir failed:");
            return false;
        }
    }
    
    return true;
}

thread_local char Logger::tmpBuffer_[Logger::kMaxCharPerLog];
thread_local std::size_t Logger::pos_ = kPrefixLevelLen + kPrefixTimeLen;
thread_local int64_t Logger::lastLogSecond_ = -1;
thread_local int64_t Logger::lastLogMSecond_ = -1;
thread_local unsigned int Logger::curLevel_ = 0;
thread_local char Logger::tid_[16] = "";
thread_local int Logger::tidLen_ = 0;

unsigned int Logger::seq_ = 0;

Logger::Logger() : shutdown_(false),
                   level_(0),
                   dest_(0)
{
    _Reset();
}

Logger::~Logger()
{
    _CloseLogFile();
}

bool Logger::Init(unsigned int level, unsigned int dest, const char* dir)
{
    level_      = level;
    dest_       = dest;
    directory_  = dir ? dir : ".";
    if (directory_.back() == '/')
        directory_.pop_back();

    if (0 == level_) 
        return  true;
  
    if (dest_ & logFile)
    {
        return directory_ == "." ||
               MakeDir(directory_.c_str());
    }

    if (!(dest_ & logConsole))
    {
        std::cerr << "log has no output, but loglevel is " << level << std::endl;
        return false;
    }
            
    return true;
}

bool Logger::_CheckChangeFile()
{
    if (!file_.IsOpen())
        return true;
    
    return file_.Offset() + kMaxCharPerLog > kDefaultLogSize;
}

const std::string& Logger::_MakeFileName()
{ 
    char name[32]; 
    Time now; 
    size_t len = now.FormatTime(name); 
    name[len] = '\0';  

    std::ostringstream pid;
    pid << "@" << ::getpid() << "-"; 

    seq_ ++; 
    fileName_  = directory_ + "/" + name + pid.str() + std::to_string(seq_) + ".log";
            
    return fileName_; 
}

bool Logger::_OpenLogFile(const std::string& name)
{ 
    return file_.Open(name.data(), true);
}

void Logger::_CloseLogFile()
{
    return file_.Close();
}

// TODO config
static const int kFlushThreshold = 2 * 1024 * 1024;

void Logger::Flush(enum LogLevel level)
{
    assert (level == curLevel_);

    if (IsLevelForbid(curLevel_))
    {
        _Reset();
        return;
    }

    if (!(level & curLevel_) ||
         (pos_ < kPrefixTimeLen + kPrefixLevelLen))
    {
        assert (false);
        return;
    }
         
    if (pos_ == kPrefixTimeLen + kPrefixLevelLen)
        return; // empty log
    
    Time now;

    auto seconds = now.MilliSeconds() / 1000;
    if (seconds != lastLogSecond_)
    {
        now.FormatTime(tmpBuffer_);
        lastLogSecond_ = seconds;
    }
    else
    {
        auto msec = now.MicroSeconds() % 1000000;
        if (msec != lastLogMSecond_)
        {
            snprintf(tmpBuffer_ + 20, 7, "%06d", static_cast<int>(msec));
            tmpBuffer_[26] = ']';
            lastLogMSecond_ = msec;
        }
    }

    switch(level)
    {
    case logINFO:
        memcpy(tmpBuffer_ + kPrefixTimeLen, "[INF]:", kPrefixLevelLen);
        break;

    case logDEBUG:
        memcpy(tmpBuffer_ + kPrefixTimeLen, "[DBG]:", kPrefixLevelLen);
        break;

    case logWARN:
        memcpy(tmpBuffer_ + kPrefixTimeLen, "[WRN]:", kPrefixLevelLen);
        break;

    case logERROR:
        memcpy(tmpBuffer_ + kPrefixTimeLen, "[ERR]:", kPrefixLevelLen);
        break;

    case logUSR:
        memcpy(tmpBuffer_ + kPrefixTimeLen, "[USR]:", kPrefixLevelLen);
        break;

    default:    
        memcpy(tmpBuffer_ + kPrefixTimeLen, "[???]:", kPrefixLevelLen);
        break;
    }

    if (tidLen_ == 0)
    {
        std::ostringstream oss;
        oss << std::this_thread::get_id();

        const auto& str = oss.str();
        tidLen_ = std::min<int>(str.size(), sizeof tid_);
        tid_[0] = '|'; // | thread_id
        memcpy(tid_ + 1, str.data(), tidLen_);
        tidLen_ += 1;
    }

    // Put tid_ at tail, because tid_ len vary from different platforms
    memcpy(tmpBuffer_ + pos_, tid_, tidLen_);
    pos_ += tidLen_;

    tmpBuffer_[pos_ ++] = '\n';
    tmpBuffer_[pos_] = '\0';

    BufferInfo* info = nullptr;
    {
        std::unique_lock<std::mutex> guard(mutex_);
        if (shutdown_)
        {
            std::cout << tmpBuffer_;
            return;
        }

        info = buffers_[std::this_thread::get_id()].get();
        if (!info)
            buffers_[std::this_thread::get_id()].reset(info = new BufferInfo());

        assert (!info->inuse_);
        info->inuse_ = true;
    }

    // Format: level info, length, log msg
    int logLevel = level;

    info->buffer_.PushData(&logLevel, sizeof logLevel);
    info->buffer_.PushData(&pos_, sizeof pos_);
    info->buffer_.PushData(tmpBuffer_, pos_);

    _Reset();

    if (info->buffer_.ReadableSize() > kFlushThreshold)
    {
        info->inuse_ = false;
        LogManager::Instance().AddBusyLog(this);
    }
    else
    {
        info->inuse_ = false;
    }
}

void Logger::_Color(unsigned int color)
{
    const char* colorstrings[Max] = {
        "",
        "\033[1;31;40m",
        "\033[1;32;40m",
        "\033[1;33;40m",
        "\033[0m",
        "\033[1;34;40m",
        "\033[1;35;40m",
        "\033[1;37;40m",
    };

    fprintf(stdout, "%s", colorstrings[color]);
}

Logger&  Logger::operator<< (const char* msg)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    const auto len = strlen(msg);
    if (pos_ + len >= kMaxCharPerLog)
        return *this;

    memcpy(tmpBuffer_ + pos_, msg, len);
    pos_ += len;

    return  *this;
}

Logger&  Logger::operator<< (const unsigned char* msg)
{
    return operator<<(reinterpret_cast<const char*>(msg));
}

Logger&  Logger::operator<< (const std::string& msg)
{
    return operator<<(msg.c_str());
}

Logger&  Logger::operator<< (void* ptr)
{
    if (IsLevelForbid(curLevel_))
        return *this;
    
    if (pos_ + 18 < kMaxCharPerLog)
    {
        unsigned long ptrValue = (unsigned long)ptr;
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%#018lx", ptrValue);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}


Logger&  Logger::operator<< (unsigned char a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 3 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%hhd", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}


Logger&  Logger::operator<< (char a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 3 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%hhu", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned short a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 5 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%hu", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (short a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 5 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%hd", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned int a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 10 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%u", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (int a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 10 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%d", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned long a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 20 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%lu", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (long a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 20 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%ld", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned long long a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 20 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%llu", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (long long a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 20 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%lld", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (double a)
{
    if (IsLevelForbid(curLevel_))
        return *this;

    if (pos_ + 20 < kMaxCharPerLog)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, kMaxCharPerLog - pos_, "%.6g", a);
        if (nbytes > 0) pos_ += nbytes;
    }
    
    return  *this;
}


bool Logger::Update()
{
    std::vector<std::unique_ptr<BufferInfo> > tmpBufs;

    bool todo = false;
    {
        std::unique_lock<std::mutex> guard(mutex_);

        for (auto it(buffers_.begin()); it != buffers_.end(); )
        {
            assert (it->second);

            if (it->second->inuse_)
            {
                // if logs is still in producing, there will be some work to do.
                todo = true;
                ++ it;
            }
            else
            {
                tmpBufs.push_back(std::move(it->second));
                it = buffers_.erase(it);
            }
        }
    }

    for (auto& pbuf : tmpBufs)
    {
        const char* data = pbuf->buffer_.ReadAddr();
        const auto size = pbuf->buffer_.ReadableSize();
        auto nWritten = _Log(data, size);
        assert (nWritten == size);
    }

    file_.Sync();

    return todo;
}

void   Logger::_Reset()
{
    curLevel_ = 0;
    pos_  = kPrefixLevelLen + kPrefixTimeLen ;
}

size_t  Logger::_Log(const char* data, size_t dataLen)
{
    const auto minLogSize = sizeof(int) + sizeof(size_t);

    size_t nOffset = 0;
    while (nOffset + minLogSize < dataLen)
    {
        int level = *(int*)(data + nOffset);
        size_t len = *(size_t* )(data + nOffset + sizeof(int));
        if (dataLen < nOffset + minLogSize + len)
        {
            std::cerr << "_WriteLog skip 0!!!\n ";
            break;
        }

        _WriteLog(level, len, data + nOffset + minLogSize);
        nOffset += minLogSize + len;
    }

    return nOffset;
}


void Logger::_WriteLog(int level, size_t len, const char* data)
{
    assert (len > 0 && data);
    
    if (dest_ & logConsole)
    {
        switch (level)
        {
        case logINFO:
            _Color(Green);
            break;

        case logDEBUG:
            _Color(White);
            break;

        case logWARN:
            _Color(Yellow);
            break;

        case logERROR:
            _Color(Red);
            break;

        case logUSR:
            _Color(Purple);
            break;

        default:
            _Color(Red);
            break;
        }

        fprintf(stdout, "%.*s", static_cast<int>(len), data);
        _Color(Normal);
    }

    if (dest_ & logFile)
    {
        while (_CheckChangeFile())
        {
            _CloseLogFile();
            if (!_OpenLogFile(_MakeFileName().c_str()))
                break;
        }

        assert (file_.IsOpen());
        file_.Write(data, len);
    }
}

void Logger::Shutdown()
{
    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_)
        return;

    shutdown_ = true;
    std::cout << "stop logger " << (void*)this << std::endl;
}

LogManager& LogManager::Instance()
{
    static LogManager mgr;
    return mgr;
}

LogManager::LogManager() : shutdown_(true)
{
    nullLog_.Init(0);
}

void LogManager::Start()
{
    std::unique_lock<std::mutex> guard(mutex_);
    assert (shutdown_);
    shutdown_ = false;

    auto io = std::bind(&LogManager::Run, this);
    iothread_ = std::thread{std::move(io)};
}

void LogManager::Stop()
{
    {
        std::unique_lock<std::mutex> guard(mutex_);
        if (shutdown_)
            return;

        shutdown_ = true;
        guard.unlock();
        cond_.notify_all();
    }
        
    {
        std::lock_guard<std::mutex> guard(logsMutex_);
        for (auto& plog : logs_)
            plog->Shutdown();
    }

    if (iothread_.joinable())
        iothread_.join();
}

std::shared_ptr<Logger> LogManager::CreateLog(unsigned int level,
                                              unsigned int dest,
                                              const char* dir)
{

    auto log(std::make_shared<Logger>());
            
    if (!log->Init(level, dest, dir))
    {   
        std::shared_ptr<Logger> nulllog(&nullLog_, [](Logger* ) {});
        return nulllog;
    }   
    else
    {  
        std::lock_guard<std::mutex> guard(logsMutex_);
        if (shutdown_)
        {
            std::cerr << "Warning: Please call LogManager::Start() first\n";
            std::shared_ptr<Logger> nulllog(&nullLog_, [](Logger* ) {});
            return nulllog;
        }

        logs_.emplace_back(log);
    }   

    return log;
}

    
void LogManager::AddBusyLog(Logger* log)
{
    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_)
        return;

    if (busyLogs_.insert(log).second)
    {
        guard.unlock();
        cond_.notify_one();
    }
}


void LogManager::Run()
{
    const std::chrono::milliseconds kFlushInterval(1);

    bool run = true;
    while (run)
    {
        std::vector<Logger* > tmpBusy;

        {
            std::unique_lock<std::mutex> guard(mutex_);
            cond_.wait_for(guard, kFlushInterval); 
            if (!busyLogs_.empty())
            {
                tmpBusy.assign(busyLogs_.begin(), busyLogs_.end());
                busyLogs_.clear();
            }

            if (shutdown_)
                run = false;
        }

        if (tmpBusy.empty())
        {
            std::unique_lock<std::mutex> guard(logsMutex_);
            for (auto& plog : logs_)
                tmpBusy.push_back(plog.get());
        }

        for (auto plog : tmpBusy)
        {
            plog->Update();
        }
    }

    std::unique_lock<std::mutex> guard(logsMutex_);
    assert (shutdown_);
    while (!logs_.empty())
    {
        for (auto it(logs_.begin()); it != logs_.end(); )
        {
            if (!(*it)->Update())
                it = logs_.erase(it);
            else
                ++ it;
        }
    }
}

LogHelper::LogHelper(LogLevel level) : level_(level) 
{
} 

Logger& LogHelper::operator=(Logger& log)
{
    log.Flush(level_);
    return log;
}

} // end namespace ananas

