
#ifndef BERT_APPLICATION_H
#define BERT_APPLICATION_H

#include <signal.h>
#include <atomic>
#include <memory>

#include "EventLoop.h"
#include "Typedefs.h"
#include "Poller.h"
#include "ananas/util/Timer.h"
#include "ananas/util/ThreadPool.h"

///@brief Namespace ananas
namespace ananas {
/// @file Application.h

///@brief Abstract for a process.
///
/// It's the app template class, should be singleton.
/// Usage:
///@code
///    auto& app = ananas::Application::Instance();
///    app.SetNumOfWorker(4);
///    app.Listen("localhost", 8000, OnNewConnection);
///    app.Run(argc, argv);
///
///    return 0;
///@endcode
class Application {
public:
    static Application& Instance();
    ~Application();

    ///@brief Run application
    ///
    /// It's a infinite loop, until SIGINT received.
    void Run(int argc, char* argv[]);
    ///@brief Exit application
    void Exit();
    ///@brief Is application exiting?
    bool IsExit() const;
    ///@brief Get default event loop
    ///
    /// The base event loop. It's created by default.
    /// If you never call [SetNumOfWorker](@ref SetNumOfWorker), base loop
    /// will be the only event loop, working for listen&connect
    /// and network io. But if you have called SetNumOfWorker, base
    /// loop will only work for listen & connect, real network io will
    /// happen in other event loops.
    ///
    ///@return pointer to default event loop
    EventLoop* BaseLoop();

    ///@brief Set init func before Run
    ///
    /// Init is executed before workers start.
    /// So just parse arguments or some else.
    /// Do NOT do anything about network IO
    void SetOnInit(std::function<bool (int, char*[])> );

    ///@brief Set exit func after Run
    ///
    /// Exit is executed as Run's last statement.
    void SetOnExit(std::function<void ()> );

    ///@brief Listener for TCP
    ///@param listenAddr The address to bind
    ///@param cb The callback for connection returned by ::accept
    ///@param bfcb The callback for ::bind, true argument imply listen success.
    void Listen(const SocketAddr& listenAddr,
                NewTcpConnCallback cb,
                BindCallback bfcb = &Application::_DefaultBindCallback);
    ///@brief Listener for TCP
    ///@param ip The ip to bind
    ///@param hostPort The port to bind, host byte order
    ///@param cb The callback for connection returned by ::accept
    ///@param bfcb The callback for ::bind, true argument imply listen success.
    void Listen(const char* ip, uint16_t hostPort,
                NewTcpConnCallback cb,
                BindCallback bfcb = &Application::_DefaultBindCallback);

    ///@brief Listener for UDP
    void ListenUDP(const SocketAddr& listenAddr,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb,
                   BindCallback bfcb = &Application::_DefaultBindCallback);
    ///@brief Listener for UDP
    void ListenUDP(const char* ip,
                   uint16_t hostPort,
                   UDPMessageCallback mcb,
                   UDPCreateCallback ccb,
                   BindCallback bfcb = &Application::_DefaultBindCallback);

    ///@brief UDP client
    void CreateClientUDP(UDPMessageCallback mcb,
                         UDPCreateCallback ccb);

    ///@brief TCP connector
    ///@param dst The address to connect
    ///@param nccb The callback for connection returned by ::connect
    ///@param cfcb The callback for connect failed
    ///@param timeout The timeout for connect
    ///@param loop When connect success, connection will put into this loop
    void Connect(const SocketAddr& dst,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max(),
                 EventLoop* loop = nullptr);

    ///@brief TCP connector
    ///@param ip The ip to connect
    ///@param hostPort The port to connect, host byte order
    ///@param nccb The callback for connection returned by ::connect
    ///@param cfcb The callback for connect failed
    ///@param timeout The timeout for connect
    ///@param loop When connect success, connection will put into this loop
    void Connect(const char* ip,
                 uint16_t hostPort,
                 NewTcpConnCallback nccb,
                 TcpConnFailCallback cfcb,
                 DurationMs timeout = DurationMs::max(),
                 EventLoop* loop = nullptr);

    ///@brief Return EventLoop by some load balance
    EventLoop* Next();
    ///@brief Set worker threads, each thread has a EventLoop object
    void SetNumOfWorker(size_t n);
    ///@brief Get worker threads's size
    size_t NumOfWorker() const;

private:
    Application();

    void _StartWorkers();

    // The default loop for accept/connect, or as worker if empty worker pool
    EventLoop base_;

    // worker thread pool
    ThreadPool pool_;
    std::vector<std::unique_ptr<EventLoop>> loops_;
    size_t numLoop_ {0};
    mutable std::atomic<size_t> currentLoop_ {0};

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

