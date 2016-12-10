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

Connector::Connector(EventLoop* loop)
{
    loop_ = loop;
}

Connector::~Connector()
{
    if (localSock_ != kInvalid)
    {
        INF(internal::g_debug) << __FUNCTION__ << localSock_ ;
        CloseSocket(localSock_);
    }
}

void Connector::SetNewConnCallback(EventLoop::NewConnCallback cb)
{
    this->newConnCallback_ = std::move(cb);
}

void Connector::SetFailCallback(EventLoop::ConnFailCallback cb)
{
    onConnectFail_ = std::move(cb);
}

bool Connector::Connect(const SocketAddr& addr)
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
            INF(internal::g_debug) << "EINPROGRESS : client socket " << localSock_<< " connected to " << peer_.GetIP() << ":" << peer_.GetPort();

            state_ = ConnectState::connecting;
            loop_->Register(eET_Write, this);
            return true;
        }

        _OnFailed();
        return false;
    }

    return true;
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

        ERR(internal::g_debug) << "HandleWriteEvent failed: clientsocket " << localSock_ << " connected to " << peer_.GetPort() << ", error is " << error;
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

    state_  = ConnectState::connected;
    INF(internal::g_debug) << "Connect success! Socket " << localSock_ << ", connected to port " << peer_.GetPort();

    std::unique_ptr<Connection> conn(new Connection(loop_));
    conn->Init(localSock_, peer_);
    conn->SetFailCallback(onConnectFail_);
    this->newConnCallback_(conn.get());

    this->localSock_ = kInvalid;
    loop_->Unregister(eET_Write, this);

    conn->OnConnect();
    loop_->Register(eET_Read | eET_Write, conn.release());
}

void Connector::_OnFailed()
{
    assert (state_ != ConnectState::connected);

    if (state_ == ConnectState::failed)
        return;


    INF(internal::g_debug) << "Failed client socket " << localSock_ << " connected to " << peer_.GetIP();

    if (onConnectFail_) 
        onConnectFail_(loop_, peer_);

    if (state_ != ConnectState::none)
        loop_->Unregister(eET_Write, this);

    state_ = ConnectState::failed;
}

} // end namespace internal
} // end namespace ananas

