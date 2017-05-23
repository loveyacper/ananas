#include <errno.h>

#include <cstring>
#include <cassert>
#include "EventLoop.h"
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
    this->newConnCallback_ = std::move(cb);
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
        INF(internal::g_debug) << "Already connect or connecting " << peer_.GetPort();
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
                                   << " connected to " << peer_.GetIP() << ":" << peer_.GetPort();

            state_ = ConnectState::connecting;
            loop_->Register(eET_Write, this);

            if (timeout != DurationMs::max())
            {
                timeoutId_ = loop_->ScheduleAfter(timeout, [this]() {
                    if (this->state_ != ConnectState::connected)
                        this->_OnFailed();
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

    if (::getsockopt(localSock_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
    {
        if (error != 0)
            errno = error;

        ERR(internal::g_debug) << "HandleWriteEvent failed: clientsocket " << localSock_
                               << " connected to " << peer_.GetPort()
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
    INF(internal::g_debug) << "Connect success! Socket " << localSock_ << ", connected to port " << peer_.GetPort();

    // create new conn
    auto c = std::make_shared<Connection>(loop_);
    c->Init(localSock_, peer_);
        
    // unregister connector
    if (oldState == ConnectState::connecting)
        this->loop_->Unregister(eET_Write, this);

    // register new conn
    if (this->loop_->Register(eET_Read, c.get()))
    {
        c->SetFailCallback(this->onConnectFail_);
        this->newConnCallback_(c.get());
        c->OnConnect();
    }
    else
    {
        ERR(internal::g_debug) << "_OnSuccess but register socket " << c->Identifier() << " failed!";
    }
}

void Connector::_OnFailed()
{
    assert (state_ != ConnectState::connected);

    const auto oldState = state_;

    if (state_ == ConnectState::failed)
        return;

    state_ = ConnectState::failed;

    INF(internal::g_debug) << "Failed client socket " << localSock_
                           << " connected to " << peer_.GetIP();

    if (onConnectFail_) 
        onConnectFail_(loop_, peer_);

    if (oldState == ConnectState::connecting)
        loop_->Unregister(eET_Write, this);
}

} // end namespace internal
} // end namespace ananas

