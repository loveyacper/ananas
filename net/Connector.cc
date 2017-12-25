#include <errno.h>

#include <cstring>
#include <cassert>
#include "EventLoop.h"
#include "EventLoopGroup.h"
#include "Connector.h"
#include "Connection.h"
#include "AnanasDebug.h"

namespace ananas
{
namespace internal
{

Connector::Connector(EventLoop* loop) :
    loop_(loop)
{
}

Connector::~Connector()
{
    if (timeoutId_)
    {
        bool succ = loop_->Cancel(timeoutId_);
        assert (succ);
    }

    if (localSock_ != kInvalid)
    {
        if (state_ != ConnectState::connected)
        {
            INF(internal::g_debug) << __FUNCTION__ << localSock_ ;
            CloseSocket(localSock_);
        }
    }
}

void Connector::SetNewConnCallback(NewTcpConnCallback cb)
{
    newConnCallback_ = std::move(cb);
}

void Connector::SetFailCallback(TcpConnFailCallback cb)
{
    onConnectFail_ = std::move(cb);
}

bool Connector::Connect(const SocketAddr& addr, DurationMs timeout)
{
    if (!addr.IsValid())
        return false;

    if (state_ != ConnectState::none)
    {
        INF(internal::g_debug) << "Already connect or connecting "
                               << peer_.ToString();
        return false;
    }

    assert (localSock_ == kInvalid);

    peer_ = addr;
    localSock_ = CreateTCPSocket();
    if (localSock_ == kInvalid)
        return false;

    SetNonBlock(localSock_);
    SetNodelay(localSock_);
    SetRcvBuf(localSock_);
    SetSndBuf(localSock_);

    int ret = ::connect(localSock_, (struct sockaddr*)&peer_, sizeof peer_);
    if (ret == 0)
    {
        _OnSuccess();
        return true;
    }
    else
    {
        if (EINPROGRESS == errno)
        {
            INF(internal::g_debug) << "EINPROGRESS : client socket " << localSock_
                                   << " connected to " << peer_.ToString();

            state_ = ConnectState::connecting;
            loop_->Register(eET_Write, shared_from_this());

            if (timeout != DurationMs::max())
            {
                timeoutId_ = loop_->ScheduleAfter<1>(timeout, [this]() {
                    if (this->state_ != ConnectState::connected) {
                        this->timeoutId_.reset();
                        this->_OnFailed();
                    }
                });
            }

            return true;
        }

        _OnFailed();
        return false;
    }

    return false; // never here
}

int Connector::Identifier() const
{
    return localSock_;
}

bool Connector::HandleReadEvent()
{
    assert (false);
    return false;
}

bool Connector::HandleWriteEvent()
{
    int error = 0;
    socklen_t len = sizeof(error);

    if (::getsockopt(localSock_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 ||
                     error != 0)
    {
        if (error != 0)
            errno = error;

        ERR(internal::g_debug) << "HandleWriteEvent failed: clientsocket " << localSock_
                               << " connected to " << peer_.ToString()
                               << ", error is " << error;
        return false;
    }
        
    _OnSuccess();
    return true;
}

void Connector::HandleErrorEvent()
{ 
    if (localSock_ == kInvalid)
        return;

    _OnFailed();
}

void Connector::_OnSuccess()
{
    assert (state_ != ConnectState::connected);

    const auto oldState = state_;
    state_  = ConnectState::connected;
    INF(internal::g_debug) << "Connect success! Socket " << localSock_
                           << ", connected to port " << peer_.ToString();

    auto loop = loop_->Parent()->SelectLoop();
    int connfd = localSock_;
    auto onFail = std::move(onConnectFail_);
    auto newCb = std::move(newConnCallback_);
    auto peer = peer_;
    // unregister connector
    if (oldState == ConnectState::connecting)
        this->loop_->Unregister(eET_Write, shared_from_this());
            
    auto func = [loop, connfd, peer, newCb, onFail]()
    {
        // create new conn
        auto c = std::make_shared<Connection>(loop);
        c->Init(connfd, peer);

        // register new conn
        if (loop->Register(eET_Read, c)) {
            c->SetFailCallback(std::move(onFail));
            newCb(c.get());
            c->_OnConnect();
        }
        else {
            ERR(internal::g_debug) << "_OnSuccess but register socket "
                                   << c->Identifier()
                                   << " failed!";
        }
    };
    loop->Execute(std::move(func));
}

void Connector::_OnFailed()
{
    assert (state_ != ConnectState::connected);

    const auto oldState = state_;

    if (state_ == ConnectState::failed)
        return;

    state_ = ConnectState::failed;

    INF(internal::g_debug) << "Failed client socket " << localSock_
                           << " connected to " << peer_.ToString();

    if (onConnectFail_) 
        onConnectFail_(loop_, peer_);

    if (oldState == ConnectState::connecting)
        loop_->Unregister(eET_Write, shared_from_this());
}

} // end namespace internal
} // end namespace ananas

