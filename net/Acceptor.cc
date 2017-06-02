#include <errno.h>
#include <cassert>
#include "EventLoop.h"
#include "Connection.h"
#include "Acceptor.h"

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


void Acceptor::SetNewConnCallback(NewTcpConnCallback cb)
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
            auto conn(std::make_shared<Connection>(loop_));
            conn->Init(connfd, peer_);

            // if send huge data OnConnect, may call Modify for epoll_out, so register events first
            if (loop_->Register(eET_Read, conn.get()))
            {
                newConnCallback_(conn.get());
                conn->_OnConnect();
            }
            else
            {
                ERR(internal::g_debug) << "Failed to register socket " << conn->Identifier();
            }
        }
        else
        {
            bool goAhead = false;
            const int error = errno;
            switch (error)
            {
            //case EWOULDBLOCK:
            case EAGAIN:
                return true; // it's fine

            case EINTR:
            case ECONNABORTED:
            case EPROTO:
                goAhead = true; // should retry
                break;

            case EMFILE:
            case ENFILE:
                ERR(internal::g_debug) << "Not enough file descriptor available, error is " << error;
                ERR(internal::g_debug) << "may be CPU 100%";
                return true;

            case ENOBUFS:
            case ENOMEM:
                ERR(internal::g_debug) << "Not enough memory, limited by the socket buffer limits";
                ERR(internal::g_debug) << "may be CPU 100%";
                return true;

            case ENOTSOCK: 
            case EOPNOTSUPP:
            case EINVAL:
            case EFAULT:
            case EBADF:
            default:
                ERR(internal::g_debug) << "BUG: error = " << error;
                assert (false);
                break;
            }

            if (!goAhead)
                return false;
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
    ERR(internal::g_debug) << "Acceptor::HandleErrorEvent";
    loop_->Unregister(eET_Read, this);
}

int Acceptor::_Accept()
{
    socklen_t addrLength = sizeof peer_;
    return ::accept(localSock_, (struct sockaddr *)&peer_, &addrLength);
}

} // end namespace internal
} // end namespace ananas

