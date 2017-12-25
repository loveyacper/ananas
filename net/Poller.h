#ifndef BERT_POLLER_H
#define BERT_POLLER_H

#include <vector>
#include <memory>
#include <stdio.h>

namespace ananas
{
namespace internal
{

enum EventType
{
    eET_None  = 0,
    eET_Read  = 0x1 << 0,
    eET_Write = 0x1 << 1,
    eET_Error = 0x1 << 2,
};

class Channel : public std::enable_shared_from_this<Channel>
{
public:
    Channel()
    {
        printf("New channel %p\n", (void*)this);
    }
    virtual ~Channel()
    {
        printf("Delete channel %p\n", (void*)this);
    }

    Channel(const Channel& ) = delete;
    void operator=(const Channel& ) = delete;

    virtual int Identifier() const = 0; // the socket 

    unsigned int GetUniqueId() const { return unique_id_; }
    void SetUniqueId(unsigned int id) { unique_id_ = id; }

    virtual bool HandleReadEvent() = 0;
    virtual bool HandleWriteEvent() = 0;
    virtual void HandleErrorEvent() = 0;

private:
    unsigned int unique_id_ = 0; // dispatch by ioloop
};
    

struct FiredEvent
{
    int   events;
    void* userdata;

    FiredEvent() : events(0), userdata(nullptr)
    {
    }
};

class Poller
{
public:
    Poller() : multiplexer_(-1)
    {
    }

    virtual ~Poller()
    {
    }

    virtual bool Register(int fd, int events, void* userPtr) = 0;
    virtual bool Modify(int fd, int events, void* userPtr) = 0;
    virtual bool Unregister(int fd, int events) = 0;

    virtual int Poll(std::size_t maxEv, int timeoutMs) = 0;
    const std::vector<FiredEvent>& GetFiredEvents() const {  return firedEvents_; }

protected:
    int multiplexer_;
    std::vector<FiredEvent>  firedEvents_;
};

} // end namespace internal
} // end namespace ananas

#endif

