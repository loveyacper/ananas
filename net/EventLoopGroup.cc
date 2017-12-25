#include "Socket.h"
#include "EventLoopGroup.h"
#include "EventLoop.h"
#include "Application.h"

namespace ananas
{

EventLoopGroup::EventLoopGroup(size_t nLoops)
{
    for (size_t i = 0; i < nLoops; ++ i)
    {
        std::unique_ptr<EventLoop> loop(new EventLoop(this));
        loops_.emplace_back(std::move(loop));
    }

    Application::Instance()._AddGroup(this);
}

EventLoopGroup::~EventLoopGroup()
{
}

void EventLoopGroup::SetNumOfEventLoop(size_t n)
{
    assert (n <= 1024);

    while (loops_.size() < n)
    {
        std::unique_ptr<EventLoop> loop(new EventLoop(this));
        loops_.emplace_back(std::move(loop));
    }
}
    
void EventLoopGroup::Stop()
{
    stop_ = true;
}

bool EventLoopGroup::IsStopped() const 
{
    return stop_;
}

void EventLoopGroup::Start()
{
    int i = 0;
    for (auto& loop : loops_)
    {
        ++ i;
        pool_.Execute([this, loop = loop.get(), i]()
                      {
                          loop->Run();
                      });
    }
}

void EventLoopGroup::Wait()
{
    pool_.JoinAll();
}

void EventLoopGroup::Listen(const SocketAddr& listenAddr,
                            NewTcpConnCallback cb,
                            BindFailCallback bfcb)
{
    auto loop = SelectLoop();
    loop->Execute([loop, listenAddr, cb, bfcb]()
                  {
                    if (!loop->Listen(listenAddr, std::move(cb)))
                        bfcb(false, listenAddr);
                    else
                        bfcb(true, listenAddr);
                  });
}

void EventLoopGroup::Listen(const char* ip,
                            uint16_t hostPort,
                            NewTcpConnCallback cb,
                            BindFailCallback bfcb)
{
    SocketAddr addr(ip, hostPort);
    Listen(addr, std::move(cb), std::move(bfcb));
}

void EventLoopGroup::ListenUDP(const SocketAddr& addr,
                               UDPMessageCallback mcb,
                               UDPCreateCallback ccb,
                               BindFailCallback bfcb)
{
    auto loop = SelectLoop();
    loop->Execute([loop, addr, mcb, ccb, bfcb]()
                  {
                    if (!loop->ListenUDP(addr, std::move(mcb), std::move(ccb)))
                        bfcb(false, addr);
                    else
                        bfcb(true, addr);
                  });
}

void EventLoopGroup::ListenUDP(const char* ip, uint16_t hostPort,
                               UDPMessageCallback mcb,
                               UDPCreateCallback ccb,
                               BindFailCallback bfcb)
{
    SocketAddr addr(ip, hostPort);
    ListenUDP(addr, std::move(mcb), std::move(ccb), std::move(bfcb));
}

void EventLoopGroup::CreateClientUDP(UDPMessageCallback mcb,
                                     UDPCreateCallback ccb)
{
    auto loop = SelectLoop();
    loop->Execute([loop, mcb, ccb]()
                  {
                    loop->CreateClientUDP(std::move(mcb), std::move(ccb));
                  });
}

void EventLoopGroup::Connect(const SocketAddr& dst,
                             NewTcpConnCallback nccb,
                             TcpConnFailCallback cfcb,
                             DurationMs timeout)
{
    auto loop = SelectLoop();
    loop->Execute([loop, dst, nccb, cfcb, timeout]()
                  {
                     loop->Connect(dst,
                                   std::move(nccb),
                                   std::move(cfcb),
                                   timeout);
                  });
}

void EventLoopGroup::Connect(const char* ip,
                             uint16_t hostPort,
                             NewTcpConnCallback nccb,
                             TcpConnFailCallback cfcb,
                             DurationMs timeout)
{
    SocketAddr dst(ip, hostPort);
    Connect(dst, std::move(nccb), std::move(cfcb), timeout);
}

EventLoop* EventLoopGroup::SelectLoop()
{
    if (currentLoop_  >= loops_.size())
        currentLoop_ = 0;

    return loops_[currentLoop_++].get();
}

} // end namespace ananas

