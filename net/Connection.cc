#include <errno.h>
#include <unistd.h>

#include "EventLoop.h"
#include "Connection.h"
#include "AnanasDebug.h"

namespace ananas
{

Connection::Connection(EventLoop* loop) :
    loop_(loop),
    localSock_(kInvalid),
    maxPacketSize_(1024)
{
}

Connection::~Connection()
{
}

bool Connection::Init(int fd, const SocketAddr& peer)
{
    if (fd == kInvalid)
        return false;

    localSock_ = fd;
    SetNonBlock(localSock_);

    peer_ = peer;
    return true;
}

void Connection::SetMaxPacketSize(std::size_t  s)
{
    maxPacketSize_ = s;
}

int Connection::Identifier() const
{
    return localSock_;
}

bool Connection::HandleReadEvent()
{
    while (true)
    {
        recvBuf_.AssureSpace(maxPacketSize_);

        int bytes = ::recv(localSock_, recvBuf_.WriteAddr(), recvBuf_.WritableSize(), 0);
        if (kError == bytes && (EAGAIN == errno || EWOULDBLOCK == errno))
            return true;

        if (0 == bytes)
            return false; // eof

        if (bytes > 0)
            recvBuf_.Produce(bytes);
        else
            return false;

        while (!recvBuf_.IsEmpty())
        {
            auto bytes = onMessage_(this, recvBuf_.ReadAddr(), recvBuf_.ReadableSize());

            if (bytes == 0)
                break;
            else
                recvBuf_.Consume(bytes);
        }
    }

	return true;
}


int Connection::_Send(const void* pData, size_t len)
{
	int  bytes = ::send(localSock_, pData, len, 0);
    if (kError == bytes && (EAGAIN == errno || EWOULDBLOCK == errno))
        bytes = 0;

    return bytes;
}

bool Connection::HandleWriteEvent()
{
    if (sendBuf_.IsEmpty())
    {
        sendBuf_.Clear();
        loop_->Modify(internal::eET_Read, this);
        return true;
    }

    auto bytes = _Send(sendBuf_.ReadAddr(), sendBuf_.ReadableSize());

    if (static_cast<std::size_t>(bytes) == sendBuf_.ReadableSize())
    {
        sendBuf_.Clear();
        loop_->Modify(internal::eET_Read, this);
    }
    else if (bytes > 0)
    {
        sendBuf_.Consume(bytes);
    }

    return bytes != kError;
}

void  Connection::HandleErrorEvent()
{
    if (onDisconnect_) 
        onDisconnect_(this);

    if (onConnFail_)
        onConnFail_(loop_, peer_);

    loop_->Unregister(internal::eET_Read | internal::eET_Write, this);
}

bool Connection::SendPacket(const void* data, std::size_t  size)
{
    if (size == 0)
        return true;

    if (!sendBuf_.IsEmpty())
    {
        sendBuf_.PushData(data, size);
        loop_->Modify(internal::eET_Read | internal::eET_Write, this);
        return true;
    }
        
    auto bytes = _Send(data, size);
    if (bytes == kError)
        return false;

    if (bytes < static_cast<int>(size))
    {
        WRN(internal::g_debug) << localSock_ << " want send " << size << " bytes, but only send " << bytes;
        sendBuf_.PushData((char*)data + bytes, size - static_cast<std::size_t>(bytes));
        loop_->Modify(internal::eET_Read | internal::eET_Write, this);
    }

	return true;
}

    
void Connection::SetOnConnect(std::function<void (Connection* )> cb)
{
    onConnect_ = std::move(cb);
}

void Connection::SetOnDisconnect(std::function<void (Connection* )> cb)
{
    onDisconnect_ = std::move(cb);
}

void Connection::SetOnMessage(std::function<PacketLen_t (Connection*, const char* data, PacketLen_t len)> cb)
{
    onMessage_ = std::move(cb);
}

void Connection::OnConnect()
{
    if (onConnect_)
        onConnect_(this);
}

void Connection::SetFailCallback(EventLoop::ConnFailCallback cb)
{
    onConnFail_ = std::move(cb);
}

} // end namespace ananas

