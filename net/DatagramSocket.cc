#include <errno.h>
#include <memory>

#include "DatagramSocket.h"
#include "EventLoop.h"
#include "AnanasDebug.h"

namespace ananas
{

DatagramSocket::DatagramSocket(EventLoop* loop) :
    loop_(loop),
    localSock_(kInvalid),
    maxPacketSize_(2048)
{
}

DatagramSocket::~DatagramSocket()
{
    ANANAS_INF << "Close Udp socket " << Identifier();
    CloseSocket(localSock_);
}

bool DatagramSocket::Bind(const SocketAddr* addr)
{
    if (localSock_ != kInvalid)
    {
        ANANAS_ERR << "UDP socket repeat create";
        return false;
    }

    localSock_ = CreateUDPSocket();
    if (localSock_ == kInvalid)
    {
        ANANAS_ERR << "Failed create udp socket! Error = " << errno;
        return false;
    }

    SetNonBlock(localSock_);
    SetReuseAddr(localSock_);

    const bool isServer = (addr && addr->IsValid());
    if (isServer)
    {
        //  server UDP
        uint16_t port = addr->GetPort();
        int ret = ::bind(localSock_, (struct sockaddr*)&addr->GetAddr(), sizeof(addr->GetAddr()));
        if (kError == ret)
        {
            CloseSocket(localSock_);
            ANANAS_ERR << "cannot bind udp port " << port;
            return false;
        }
    }
    else
    {
        ; // nothing to do: client UDP
    }
            
    if (!loop_->Register(internal::eET_Read, shared_from_this()))
    {
        ANANAS_ERR << "add udp to loop failed, socket = " << localSock_;
        return false;
    }

    if (onCreate_)
        onCreate_(this);
        
    ANANAS_INF << "Create new udp fd = " << localSock_;
    return true;
}

int DatagramSocket::Identifier() const 
{
    return localSock_;
}

    
bool DatagramSocket::HandleReadEvent()
{
    std::unique_ptr<char []> recvbuf(new char[maxPacketSize_]);
    while (true)
    {
        socklen_t len = sizeof srcAddr_;
        int bytes = ::recvfrom(localSock_,
                               &recvbuf[0], maxPacketSize_,
                               0,
                               (struct sockaddr*)&srcAddr_, &len);

        if (kError == bytes && (EAGAIN == errno || EWOULDBLOCK == errno))
            return true;

        if (bytes <= 0)
        {
            ANANAS_ERR << "UDP fd " << localSock_
                       << ", HandleRead error : " << bytes 
                       << ", errno = " << errno;
            return true;
        }

        // onMessage_ is void
        onMessage_(this, &recvbuf[0], bytes);
    }

    return  true;
}

void DatagramSocket::_PutSendBuf(const void* data, size_t size, const SocketAddr* dst)
{
    Package pkg;
    pkg.dst = *dst;
    pkg.data.assign(reinterpret_cast<const char* >(data), size);

    sendList_.emplace_back(std::move(pkg));
}

int DatagramSocket::_Send(const void* data, size_t size, const SocketAddr& dst)
{
    int bytes = ::sendto(localSock_,
                         data, size,
                         0,
                         (const struct sockaddr*)&dst, sizeof dst);

    if (bytes == kError && (EAGAIN == errno || EWOULDBLOCK == errno))
    {
        ANANAS_WRN << "send wouldblock";
        return 0;
    }

    return bytes;
}

bool DatagramSocket::SendPacket(const void* data, size_t size, const SocketAddr* dst)
{
    if (size == 0 || !data)
        return true;

    if (!dst)
        dst = &srcAddr_;

    if (!sendList_.empty())
    {
        _PutSendBuf(data, size, dst);
        return true;
    }

    int bytes = _Send(data, size, *dst);
    if (bytes == 0)
    {
        _PutSendBuf(data, size, dst);
        loop_->Modify(internal::eET_Read | internal::eET_Write, shared_from_this());
        return true;
    }
    else if (bytes < 0)
    {
        ANANAS_ERR << "Fatal error when send udp to "
                   << dst->ToString()
                   << ", must skip it";
        return false;
    }

	return true;
}

bool DatagramSocket::HandleWriteEvent()
{
    while (!sendList_.empty())
    {
        const auto& pkg = sendList_.front();

        int bytes = _Send(pkg.data.data(), pkg.data.size(), pkg.dst);
        if (bytes == 0)
        {
            return true;
        }
        else if (bytes > 0)
        {
            sendList_.pop_front();
        }
        else
        {
            ANANAS_ERR << "Fatal error when send udp to "
                       << pkg.dst.ToString()
                       << ", must skip it";
            sendList_.pop_front();
        }
    }

    if (sendList_.empty())
        loop_->Modify(internal::eET_Read, shared_from_this());
        
    return true;
}

void DatagramSocket::HandleErrorEvent()
{
    ANANAS_ERR << "HandleErrorEvent";
}

} // end namespace ananas

