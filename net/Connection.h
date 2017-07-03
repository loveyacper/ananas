
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

enum class ShutdownMode
{
    eSM_Both,
    eSM_Read,
    eSM_Write,
};

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
    void ActiveClose();
    EventLoop* GetLoop() const { return loop_; }

    void Shutdown(ShutdownMode mode);
    void SetNodelay(bool enable);

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    bool SendPacket(const void* data, std::size_t len);
    bool SendPacket(const BufferVector& datum);
    bool SendPacket(const SliceVector& slice);

    void SetOnConnect(std::function<void (Connection* )> cb);
    void SetOnDisconnect(std::function<void (Connection* )> cb);
    void SetOnMessage(TcpMessageCallback cb);
    void SetFailCallback(TcpConnFailCallback cb);
    void SetOnWriteComplete(TcpWriteCompleteCallback wccb);
    void SetOnWriteHighWater(TcpWriteHighWaterCallback whwcb);

    void SetMinPacketSize(size_t s);
    void SetWriteHighWater(size_t s);

private:
    enum State
    {
        eS_None,
        eS_Connected,
        eS_CloseWaitWrite, // peer close or shutdown write, but I have data to send
        eS_PassiveClose, // should close
        eS_ActiveClose, // should close
        eS_Error,
        eS_Closed,
    };

    friend class internal::Acceptor;
    friend class internal::Connector;
    void _OnConnect();
    int _Send(const void* data, size_t len);

    EventLoop* const loop_;
    State state_ = State::eS_None;
    int localSock_;
    size_t minPacketSize_;
    size_t sendBufHighWater_;

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

