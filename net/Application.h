
#ifndef BERT_APPLICATION_H
#define BERT_APPLICATION_H

#include <signal.h>
#include <atomic>
#include <memory>

#include "EventLoop.h"
#include "Typedefs.h"
#include "Poller.h"
#include "ananas/util/Timer.h"

namespace ananas {

namespace internal {
class EventLoopGroup;
}

class Application {
public:
    static Application& Instance();
    ~Application();

    void Run(int argc, char* argv[]);
    void Exit();
    bool IsExit() const;
    EventLoop* BaseLoop();

    // Init is executed before workers start.
    // So just parse arguments or some else.
    // Do NOT do anything about network IO
    void SetOnInit(std::function<bool (int, char*[])> );

    // Exit is executed before app exit
    void SetOnExit(std::function<void ()> );

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
                 DurationMs timeout = DurationMs::max(),
                 EventLoop* loop = nullptr);

    void Connect(const char* ip,
                 uint16_t hostPort,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max(),
                 EventLoop* loop = nullptr);

    EventLoop* Next();
    void SetNumOfWorker(size_t n);
    size_t NumOfWorker() const;

private:
    Application();

    // baseGroup_ is empty, just a placeholder container for base_.
    std::unique_ptr<internal::EventLoopGroup> baseGroup_;

    // The default loop for accept/connect, or as worker if workerGroup_ is empty
    EventLoop base_;

    std::unique_ptr<internal::EventLoopGroup> workerGroup_;

    enum class State {
        eS_None,
        eS_Started,
        eS_Stopped,
    };
    std::atomic<State> state_;

    std::function<bool (int, char*[]) > onInit_;
    std::function<void ()> onExit_;

    static void _DefaultBindCallback(bool succ, const SocketAddr& );
};

} // end namespace ananas

#endif

