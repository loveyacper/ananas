#ifndef BERT_POLLER_H
#define BERT_POLLER_H

#include <vector>
#include <memory>
#include <stdio.h>

namespace ananas {
///@brief namespace internal, not exposed to user.
namespace internal {

enum EventType {
    eET_None  = 0,
    eET_Read  = 0x1 << 0,
    eET_Write = 0x1 << 1,
    eET_Error = 0x1 << 2,
};

///@brief Event source base class.
class Channel : public std::enable_shared_from_this<Channel> {
public:
    ///@brief Constructor, printf is for debug, you can comment it
    Channel() {
        printf("New channel %p\n", (void*)this);
    }
    ///@brief Destructor, printf is for debug, you can comment it
    virtual ~Channel() {
        printf("Delete channel %p\n", (void*)this);
    }

    Channel(const Channel& ) = delete;
    void operator=(const Channel& ) = delete;

    ///@brief Return socket fd for sockets, just for debug
    virtual int Identifier() const = 0;

    ///@brief The unique id, it'll not repeat in whole process.
    unsigned int GetUniqueId() const {
        return unique_id_;
    }

    ///@brief Set the unique id, it's called by library.
    void SetUniqueId(unsigned int id) {
        unique_id_ = id;
    }

    ///@brief When read event occurs
    virtual bool HandleReadEvent() = 0;
    ///@brief When write event occurs
    virtual bool HandleWriteEvent() = 0;
    ///@brief When error event occurs
    virtual void HandleErrorEvent() = 0;

private:
    unsigned int unique_id_ = 0; // dispatch by ioloop
};


struct FiredEvent {
    int   events;
    void* userdata;

    FiredEvent() : events(0), userdata(nullptr) {
    }
};

class Poller {
public:
    Poller() : multiplexer_(-1) {
    }

    virtual ~Poller() {
    }

    virtual bool Register(int fd, int events, void* userPtr) = 0;
    virtual bool Modify(int fd, int events, void* userPtr) = 0;
    virtual bool Unregister(int fd, int events) = 0;

    virtual int Poll(std::size_t maxEv, int timeoutMs) = 0;
    const std::vector<FiredEvent>& GetFiredEvents() const {
        return firedEvents_;
    }

protected:
    int multiplexer_;
    std::vector<FiredEvent>  firedEvents_;
};

} // end namespace internal
} // end namespace ananas

#endif

