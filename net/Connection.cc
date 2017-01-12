#include <cassert>

#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>

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

void Connection::SetMaxPacketSize(std::size_t  s)
{
    maxPacketSize_ = s;
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
        recvBuf_.AssureSpace(maxPacketSize_);

        int bytes = ::recv(localSock_, recvBuf_.WriteAddr(), recvBuf_.WritableSize(), 0);
        if (kError == bytes && (EAGAIN == errno || EWOULDBLOCK == errno))
            return true;

        if (0 == bytes)
            return false; // eof

        if (bytes > 0)
        {
            recvBuf_.Produce(bytes);
        }
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

    recvBuf_.Shrink();
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
        sendBuf_.Shrink();
        loop_->Modify(internal::eET_Read, this);
        return true;
    }

    auto bytes = _Send(sendBuf_.ReadAddr(), sendBuf_.ReadableSize());

    if (static_cast<std::size_t>(bytes) == sendBuf_.ReadableSize())
    {
        sendBuf_.Shrink();
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

// iovec for writev
namespace
{

struct IOVecBuffer
{
    static const int kIOVecCount = 16; // be care of IOV_MAX

    iovec iovecs[kIOVecCount];
    int iovCount = 0;

    IOVecBuffer()
    {
        Reset();
    }

    void Reset()
    {
        iovCount = 0;
    }

    bool PushBuffer(iovec v)
    {
        if (iovCount == kIOVecCount)
            return false;

        iovecs[iovCount++] = v;
        return true;
    }

    size_t TotalBytes() const
    {
        size_t total = 0;
        for (int i = 0; i < iovCount; ++ i)
            total += iovecs[i].iov_len;

        return total;
    }
};

int WriteV(int sock, const iovec* buffers, int cnt)
{
    if (cnt == 0)
        return 0;

    int bytes = static_cast<int>(::writev(sock, buffers, cnt));

    assert (bytes != 0);

    if (kError == bytes && (EAGAIN == errno || EWOULDBLOCK == errno))
        bytes = 0;

    return bytes;
}

void CollectBuffer(const iovec* buffers, int cnt, size_t skipped, Buffer& dst)
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
    IOVecBuffer buffers;
    bool save = false; // true for send, else for save

    for (const auto& e : slice)
    {
        if (save)
        {
            sendBuf_.PushData(e.data, e.len);
            continue;
        }

        iovec ivc; 
        ivc.iov_base = const_cast<void*>(e.data);
        ivc.iov_len = e.len;

        if (!buffers.PushBuffer(ivc))
        {
            assert (buffers.iovCount == IOVecBuffer::kIOVecCount);

            int ret = WriteV(localSock_, buffers.iovecs, buffers.iovCount);
            if (ret == kError)
                return false; // should close

            assert (ret >= 0);

            size_t alreadySent = static_cast<size_t>(ret);
            size_t expectSend = buffers.TotalBytes();

            if (alreadySent < expectSend ) // EAGAIN
            {
                save = true;
                CollectBuffer(buffers.iovecs, buffers.iovCount, alreadySent, sendBuf_);
            }
            else if (alreadySent == expectSend)
            {
                buffers.Reset();
            }
        }
    }

    if (!save)
    {
        int ret = WriteV(localSock_, buffers.iovecs, buffers.iovCount);
        if (ret == kError)
            return false; // should close
        
        assert (ret >= 0);

        size_t alreadySent = static_cast<size_t>(ret);
        size_t expectSend = buffers.TotalBytes();

        if (alreadySent < expectSend)
            CollectBuffer(buffers.iovecs, buffers.iovCount, alreadySent, sendBuf_);
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

