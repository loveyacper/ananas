#ifndef BERT_EVENTLOOPGROUP_H
#define BERT_EVENTLOOPGROUP_H

#include <memory>
#include <vector>

#include "ThreadPool.h"
#include "Typedefs.h"
#include "Poller.h"
#include "Timer.h"

namespace ananas
{

class EventLoop;

class EventLoopGroup
{
public:
    explicit
    EventLoopGroup(size_t nLoops = 1);
   ~EventLoopGroup();

    EventLoopGroup(const EventLoopGroup& ) = delete;
    void operator=(const EventLoopGroup& ) = delete;

    EventLoopGroup(EventLoopGroup&& ) = delete;
    void operator=(EventLoopGroup&& ) = delete;

    void SetNumOfEventLoop(size_t n);

    void Stop();
    bool IsStopped() const;

    void Start();
    void Wait();

    // listener
    void Listen(const SocketAddr& listenAddr,
                NewTcpConnCallback cb,
                BindFailCallback bfcb);
    void Listen(const char* ip, uint16_t hostPort,
                NewTcpConnCallback cb,
                BindFailCallback bfcb);

    void ListenUDP(const SocketAddr& listenAddr,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb,
                   BindFailCallback bfcb);
    void ListenUDP(const char* ip,
                   uint16_t hostPort,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb,
                   BindFailCallback bfcb);

    // udp client
    void CreateClientUDP(UDPMessageCallback mcb,
                         UDPCreateCallback ccb);

    // connector 
    void Connect(const SocketAddr& dst,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max());

    void Connect(const char* ip,
                 uint16_t hostPort,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max());

    EventLoop* SelectLoop();

private:
    void* operator new(std::size_t ); // stack only

    std::atomic<bool> stop_ {false};

    ThreadPool pool_;
    std::vector<std::unique_ptr<EventLoop> > loops_;
    size_t currentLoop_ {0};
};

} // end namespace ananas

#endif

