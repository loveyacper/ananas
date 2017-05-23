
#ifndef BERT_CONNECTION_H
#define BERT_CONNECTION_H

#include <sys/types.h>
#include "Socket.h"
#include "Poller.h"
#include "Buffer.h"
#include "Typedefs.h"

namespace ananas
{

namespace internal
{
class Acceptor;
class Connector;
}

class Connection : public internal::EventSource
{
public:
    explicit
    Connection(EventLoop* loop);
    ~Connection();

    Connection(const Connection& ) = delete;
    void operator= (const Connection& ) = delete;

    bool Init(int sock, const SocketAddr& peer);
    void SetMaxPacketSize(std::size_t s);
    const SocketAddr& Peer() const { return peer_; }
    void Close();
    EventLoop* GetLoop() const { return loop_; }

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    bool SendPacket(const void* data, std::size_t len);
    bool SendPacket(const BufferVector& datum);
    bool SendPacket(const SliceVector& slice);

    bool WriteWouldblock() const;

    void SetOnConnect(std::function<void (Connection* )> cb);
    void SetOnDisconnect(std::function<void (Connection* )> cb);
    void SetOnMessage(TcpMessageCallback cb);
    void SetFailCallback(TcpConnFailCallback cb);
    void SetOnWriteComplete(TcpWriteCompleteCallback wccb);
    void SetOnWriteHighWater(TcpWriteHighWaterCallback whwcb);

    void SetMinPacketSize(size_t s);
    void SetWriteHighWater(size_t s);

private:
    friend class internal::Acceptor;
    friend class internal::Connector;
    void OnConnect();
    int _Send(const void* data, size_t len);

    EventLoop* const loop_;
    int localSock_;
    size_t minPacketSize_;
    size_t sendBufHighWater_;
    bool valid_;

    Buffer recvBuf_;
    BufferVector sendBuf_;

    SocketAddr peer_;

    std::function<void (Connection* )> onConnect_;
    std::function<void (Connection* )> onDisconnect_;
    
    TcpMessageCallback onMessage_;
    TcpConnFailCallback onConnFail_;
    TcpWriteCompleteCallback onWriteComplete_;
    TcpWriteHighWaterCallback onWriteHighWater;
};

} // end namespace ananas

#endif

