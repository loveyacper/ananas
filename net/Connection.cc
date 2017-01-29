#include <cassert>

#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>

#include "EventLoop.h"
#include "Connection.h"
#include "AnanasDebug.h"
#include "util/Util.h"

namespace ananas
{

Connection::Connection(EventLoop* loop) :
    loop_(loop),
    localSock_(kInvalid),
    minPacketSize_(0),
    sendBufHighWater_(10 * 1024 * 1024)
{
}

Connection::~Connection()
{
    Close();
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

void Connection::Close()
{
    CloseSocket(localSock_);
}

int Connection::Identifier() const
{
    return localSock_;
}

bool Connection::HandleReadEvent()
{
    while (true)
    {
        recvBuf_.AssureSpace(4 * 1024);

        int bytes = ::recv(localSock_, recvBuf_.WriteAddr(), recvBuf_.WritableSize(), 0);
        if (kError == bytes)
        {
            if (EAGAIN == errno || EWOULDBLOCK == errno)
                return true;

            if (EINTR == errno)
                continue; // restart ::recv
        }

        if (0 == bytes)
            return false; // eof

        if (bytes > 0)
            recvBuf_.Produce(bytes);
        else
            return false;

        while (recvBuf_.ReadableSize() >= minPacketSize_)
        {
            auto bytes = onMessage_(this, recvBuf_.ReadAddr(), recvBuf_.ReadableSize());

            if (bytes == 0)
                break;
            else
                recvBuf_.Consume(bytes);
        }
    }

    recvBuf_.Shrink();
	return true;
}


int Connection::_Send(const void* data, size_t len)
{
    if (len == 0)
        return 0;

	int  bytes = ::send(localSock_, data, len, 0);
    if (kError == bytes)
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            bytes = 0;

        if (EINTR == errno)
            bytes = 0; // later try ::send
    }

    return bytes;
}
    
bool Connection::HandleWriteEvent()
{
    auto bytes = _Send(sendBuf_.ReadAddr(), sendBuf_.ReadableSize());
    if (bytes == kError)
        return false;

    sendBuf_.Consume(bytes);

    if (sendBuf_.IsEmpty())
    {
        sendBuf_.Shrink();
        loop_->Modify(internal::eET_Read, this);

        if (onWriteComplete_)
            onWriteComplete_(this);
    }

    return true;
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

    const size_t oldSendBytes = sendBuf_.ReadableSize();
    ANANAS_DEFER
    {
        size_t nowSendBytes = sendBuf_.ReadableSize();
        if (oldSendBytes < sendBufHighWater_ &&
            nowSendBytes >= sendBufHighWater_)
        {
            if (onWriteHighWater)
                onWriteHighWater(this, nowSendBytes);
        }
    };

    if (!sendBuf_.IsEmpty())
    {
        sendBuf_.PushData(data, size);
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
    else
    {
        if (onWriteComplete_)
            onWriteComplete_(this);
    }

	return true;
}

// iovec for writev
namespace
{

struct IOVecBuffer
{
    std::vector<iovec> iovecs;

    IOVecBuffer()
    {
        Reset();
    }

    void Reset()
    {
        iovecs.clear();
    }

    void PushBuffer(iovec v)
    {
        iovecs.push_back(v);
    }

    size_t TotalBytes() const
    {
        size_t total = 0;
        for (const auto& e : iovecs)
            total += e.iov_len;

        return total;
    }
};

int WriteV(int sock, const std::vector<iovec>& buffers, int cnt)
{
    const int kIOVecCount = 16; // be care of IOV_MAX

    int sent = 0;
    while (cnt > 0)
    {
        const int vc = std::min(cnt, kIOVecCount);
        int bytes = static_cast<int>(::writev(sock, &buffers[0], vc));
        cnt -= vc;

        assert (bytes != 0);

        if (kError == bytes)
        {
            if (EAGAIN == errno || EWOULDBLOCK == errno)
                return sent;

            if (EINTR == errno)
                return sent;

            return kError;  // can not send any more
        }
        else
        {
            assert (bytes > 0);
            sent += bytes;
        }
    }

    return sent;
}

void CollectBuffer(const std::vector<iovec>& buffers, int cnt, size_t skipped, Buffer& dst)
{
    for (int i = 0; i < cnt; ++ i)
    {
        if (skipped >= buffers[i].iov_len)
        {
            skipped -= buffers[i].iov_len;
        }
        else
        {
            const char* data = (const char*)buffers[i].iov_base;
            size_t len = buffers[i].iov_len;

            dst.PushData(data + skipped, len - skipped); 
            if (skipped != 0)
                skipped = 0;
        }
    }
}

} // end namespace


bool Connection::SendPacket(const BufferVector& data)
{
    SliceVector s;
    for (const auto& d : data)
    {
        s.PushBack(const_cast<Buffer&>(d).ReadAddr(), d.ReadableSize());
    }

    return SendPacket(s);
}

bool Connection::SendPacket(const SliceVector& slice)
{
    if (slice.Empty())
        return true;

    const size_t oldSendBytes = sendBuf_.ReadableSize();
    ANANAS_DEFER
    {
        size_t nowSendBytes = sendBuf_.ReadableSize();
        if (oldSendBytes < sendBufHighWater_ &&
            nowSendBytes >= sendBufHighWater_)
        {
            if (onWriteHighWater)
                onWriteHighWater(this, nowSendBytes);
        }
    };

    IOVecBuffer buffers;
    for (const auto& e : slice)
    {
        if (e.len == 0)
            continue;

        iovec ivc; 
        ivc.iov_base = const_cast<void*>(e.data);
        ivc.iov_len = e.len;

        buffers.PushBuffer(ivc);
    }

    if (!sendBuf_.IsEmpty())
    {
        CollectBuffer(buffers.iovecs, static_cast<int>(buffers.iovecs.size()), 0, sendBuf_);
        return true;
    }
            
    int ret = WriteV(localSock_, buffers.iovecs, static_cast<int>(buffers.iovecs.size()));
    if (ret == kError)
        return false; // should close

    assert (ret >= 0);

    size_t alreadySent = static_cast<size_t>(ret);
    size_t expectSend = buffers.TotalBytes();

    if (alreadySent < expectSend)
    {
        CollectBuffer(buffers.iovecs, static_cast<int>(buffers.iovecs.size()), alreadySent, sendBuf_);
        loop_->Modify(internal::eET_Read | internal::eET_Write, this);
    }
    else
    {
        if (onWriteComplete_)
            onWriteComplete_(this);
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

void Connection::SetOnMessage(TcpMessageCallback cb)
{
    onMessage_ = std::move(cb);
}

void Connection::OnConnect()
{
    if (onConnect_)
        onConnect_(this);
}

void Connection::SetFailCallback(TcpConnFailCallback cb)
{
    onConnFail_ = std::move(cb);
}

void Connection::SetOnWriteComplete(TcpWriteCompleteCallback wccb)
{
    onWriteComplete_ = std::move(wccb);
}

void Connection::SetMinPacketSize(size_t s)
{
    minPacketSize_ = s;
}

void Connection::SetWriteHighWater(size_t s)
{
    sendBufHighWater_ = s;
}

void Connection::SetOnWriteHighWater(TcpWriteHighWaterCallback whwcb)
{
    onWriteHighWater = std::move(whwcb);
}

} // end namespace ananas

