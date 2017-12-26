
#ifndef BERT_APPLICATION_H
#define BERT_APPLICATION_H

#include <signal.h>
#include <atomic>
#include <memory>

#include "EventLoop.h"
#include "Typedefs.h"
#include "Poller.h"
#include "Timer.h"

namespace ananas
{
    
class EventLoopGroup;

class Application
{
    friend class EventLoopGroup;
public:
    static Application& Instance();
    ~Application();

    void Run();
    void Exit();
    bool IsExit() const;
    EventLoop* BaseLoop();

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

    EventLoop* Next();
    void SetWorkerGroup(EventLoopGroup* group);

private:
    Application();

    std::unique_ptr<EventLoopGroup> baseGroup_;
    EventLoop base_;
    EventLoopGroup* worker_;

    enum class State
    {
        eS_None,
        eS_Started,
        eS_Stopped,
    };
    std::atomic<State> state_;
};

} // end namespace ananas

#endif

