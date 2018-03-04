
#ifndef BERT_APPLICATION_H
#define BERT_APPLICATION_H

#include <signal.h>
#include <atomic>
#include <memory>

#include "EventLoop.h"
#include "Typedefs.h"
#include "Poller.h"
#include "util/Timer.h"

namespace ananas
{
    
namespace internal
{
class EventLoopGroup;
}

class Application
{
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
                BindCallback bfcb = &Application::_DefaultBindCallback);
    void Listen(const char* ip, uint16_t hostPort,
                NewTcpConnCallback cb,
                BindCallback bfcb = &Application::_DefaultBindCallback);

    void ListenUDP(const SocketAddr& listenAddr,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb,
                   BindCallback bfcb = &Application::_DefaultBindCallback);
    void ListenUDP(const char* ip,
                   uint16_t hostPort,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb,
                   BindCallback bfcb = &Application::_DefaultBindCallback);

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
    void SetNumOfWorker(size_t n);
    size_t NumOfWorker() const;

private:
    Application();

    std::unique_ptr<internal::EventLoopGroup> baseGroup_;
    EventLoop base_;
    std::unique_ptr<internal::EventLoopGroup> workerGroup_;

    enum class State
    {
        eS_None,
        eS_Started,
        eS_Stopped,
    };
    std::atomic<State> state_;

    static void _DefaultBindCallback(bool succ, const SocketAddr& );
};

} // end namespace ananas

#endif

