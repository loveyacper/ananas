#include <errno.h>
#include <cassert>
#include "Acceptor.h"
#include "Connection.h"

#include "AnanasDebug.h"


namespace ananas
{
namespace internal
{

const int Acceptor::kListenQueue = 1024;

Acceptor::Acceptor(EventLoop* loop) :
    localSock_(kInvalid),
    localPort_(SocketAddr::kInvalidPort),
    loop_(loop)
{
}

Acceptor::~Acceptor()
{
    CloseSocket(localSock_);
    INF(internal::g_debug) << "Close Acceptor " << localPort_ ;
}


void Acceptor::SetNewConnCallback(EventLoop::NewConnCallback cb)
{
    this->newConnCallback_ = std::move(cb);
}

bool Acceptor::Bind(const SocketAddr& addr)
{
    if (!addr.IsValid())
        return false;

    if (localSock_ != kInvalid)
    {
        ERR(internal::g_debug) << "Already listen " << localPort_;
        return false;
    }

    localSock_ = CreateTCPSocket();
    if (localSock_ == kInvalid)
        return false;

    localPort_ = addr.GetPort();

    SetNonBlock(localSock_);
    SetNodelay(localSock_);
    SetReuseAddr(localSock_);
    SetRcvBuf(localSock_);
    SetSndBuf(localSock_);

    auto serv = addr.GetAddr();

    int ret = ::bind(localSock_, (struct sockaddr*)&serv, sizeof serv);
    if (kError == ret)
    {
        ERR(internal::g_debug) << "Cannot bind to port " << addr.GetPort()
                               << ", IP " << addr.GetIP();
        return false;
    }

    ret = ::listen(localSock_, kListenQueue);
    if (kError == ret)
    {
        ERR(internal::g_debug) << "Cannot listen port " << addr.GetPort();
        return false;
    }

    if (!loop_->Register(eET_Read, this))
        return false;

    INF(internal::g_debug) << "Create listen socket " << localSock_ << " on port " << localPort_;
    return  true;
}

    
int Acceptor::Identifier() const
{
    return localSock_;
}

bool Acceptor::HandleReadEvent() 
{
    while (true)
    {
        int connfd = _Accept();
        if (connfd != kInvalid)
        {
            std::unique_ptr<Connection> conn(new Connection(loop_));
            conn->Init(connfd, peer_);
            this->newConnCallback_(conn.get());

            conn->OnConnect();
            loop_->Register(eET_Read | eET_Write, conn.release());
        }
        else
        {
            bool result = false;
            switch (errno)
            {
            case EWOULDBLOCK:
            case ECONNABORTED:
            case EINTR:
                result = true;
                break;

            case EMFILE:
            case ENFILE:
                ERR(internal::g_debug) << "Not enough file descriptor available!!!";
                result = true;
                break;

            case ENOBUFS:
                ERR(internal::g_debug) << "Not enough memory\n";
                result = true;

            default:
                ERR(internal::g_debug) << "When accept, unknown error happened : " << errno;
                break;
            }

            return result;
        }
    }
    
    return true;
}

bool Acceptor::HandleWriteEvent()
{
    assert (false);
    return false;
}

void Acceptor::HandleErrorEvent()
{
    loop_->Unregister(eET_Read | eET_Write, this);
    loop_->Stop();
}

int Acceptor::_Accept()
{
    socklen_t addrLength = sizeof peer_;
    return ::accept(localSock_, (struct sockaddr *)&peer_, &addrLength);
}

} // end namespace internal
} // end namespace ananas

