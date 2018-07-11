
#include <signal.h>
#include <cstring>
#include <cstdio>

#include "util/Util.h"
#include "Application.h"
#include "AnanasLogo.h"
#include "Socket.h"
#include "EventLoopGroup.h"
#include "AnanasDebug.h"

static void SignalHandler(int num)
{
    ananas::Application::Instance().Exit();
}

static void InitSignal()
{
    struct sigaction sig;
    ::memset(&sig, 0, sizeof(sig));
   
    sig.sa_handler = SignalHandler;
    sigaction(SIGINT, &sig, NULL);
                                  
    // ignore sigpipe
    sig.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig, NULL);

#ifdef ANANAS_LOGO
    // logo
    printf("%s\n", ananas::internal::logo);
#endif
}


namespace ananas
{

Application::~Application()
{
}

Application& Application::Instance()
{
    static Application app;
    return app;
}

void Application::SetNumOfWorker(size_t num)
{
    assert (state_ == State::eS_None);
    workerGroup_->SetNumOfEventLoop(num);
}

size_t Application::NumOfWorker() const
{
    // plus one : the baseLoop
    return 1 + workerGroup_->Size();
}

void Application::Run()
{
    ANANAS_DEFER
    {
        // thread safe exit
        LogManager::Instance().Stop();
    };

    if (state_ != State::eS_None)
        return;

    state_ = State::eS_Started;
    workerGroup_->Start();

    BaseLoop()->Run();

    baseGroup_->Wait();
    printf("Stopped BaseEventLoopGroup ...\n");

    workerGroup_->Wait();
    printf("Stopped WorkerEventLoopGroup...\n");
}

void Application::Exit()
{
    if (state_ == State::eS_Stopped)
        return;

    state_ = State::eS_Stopped;
    baseGroup_->Stop();
    workerGroup_->Stop();
}

bool Application::IsExit() const
{
    return state_ == State::eS_Stopped;
}
    
EventLoop* Application::BaseLoop()
{
    return &base_;
}

void Application::Listen(const SocketAddr& listenAddr,
                         NewTcpConnCallback cb,
                         BindCallback bfcb)
{
    auto loop = BaseLoop();
    loop->Execute([loop, listenAddr, cb, bfcb]()
                  {
                    if (!loop->Listen(listenAddr, std::move(cb)))
                        bfcb(false, listenAddr);
                    else
                        bfcb(true, listenAddr);
                  });
}

void Application::Listen(const char* ip,
                         uint16_t hostPort,
                         NewTcpConnCallback cb,
                         BindCallback bfcb)
{
    SocketAddr addr(ip, hostPort);
    Listen(addr, std::move(cb), std::move(bfcb));
}

void Application::ListenUDP(const SocketAddr& addr,
                            UDPMessageCallback mcb,
                            UDPCreateCallback ccb,
                            BindCallback bfcb)
{
    auto loop = BaseLoop();
    loop->Execute([loop, addr, mcb, ccb, bfcb]()
                  {
                    if (!loop->ListenUDP(addr, std::move(mcb), std::move(ccb)))
                        bfcb(false, addr);
                    else
                        bfcb(true, addr);
                  });
}

void Application::ListenUDP(const char* ip, uint16_t hostPort,
                            UDPMessageCallback mcb,
                            UDPCreateCallback ccb,
                            BindCallback bfcb)
{
    SocketAddr addr(ip, hostPort);
    ListenUDP(addr, std::move(mcb), std::move(ccb), std::move(bfcb));
}

void Application::CreateClientUDP(UDPMessageCallback mcb,
                                  UDPCreateCallback ccb)
{
    auto loop = BaseLoop();
    loop->Execute([loop, mcb, ccb]()
                  {
                    loop->CreateClientUDP(std::move(mcb), std::move(ccb));
                  });
}

void Application::Connect(const SocketAddr& dst,
                          NewTcpConnCallback nccb,
                          TcpConnFailCallback cfcb,
                          DurationMs timeout,
                          EventLoop* dstLoop)
{
    auto loop = BaseLoop();
    loop->Execute([loop, dst, nccb, cfcb, timeout, dstLoop]()
                  {
                     loop->Connect(dst,
                                   std::move(nccb),
                                   std::move(cfcb),
                                   timeout,
                                   dstLoop);
                  });
}

void Application::Connect(const char* ip,
                          uint16_t hostPort,
                          NewTcpConnCallback nccb,
                          TcpConnFailCallback cfcb,
                          DurationMs timeout,
                          EventLoop* dstLoop)
{
    SocketAddr dst(ip, hostPort);
    Connect(dst, std::move(nccb), std::move(cfcb), timeout, dstLoop);
}

    
EventLoop* Application::Next()
{
    //assert (BaseLoop()->IsInSameLoop());
    auto loop = workerGroup_->Next();
    if (loop)
        return loop;

    return BaseLoop();
}

Application::Application() :
    baseGroup_(new internal::EventLoopGroup(0)),
    base_(baseGroup_.get()),
    workerGroup_(new internal::EventLoopGroup(0)),
    state_ {State::eS_None}
{
    InitSignal();
}

void Application::_DefaultBindCallback(bool succ, const SocketAddr& listenAddr)
{
    if (succ)
    {
        ANANAS_INF << "Listen succ for " << listenAddr.ToString();
    }
    else
    {
        ANANAS_ERR << "Listen failed for " << listenAddr.ToString();
        Application::Instance().Exit();
    }
}

} // end namespace ananas

