
#ifndef BERT_CONNECTION_H
#define BERT_CONNECTION_H

#include <sys/types.h>
#include <string>

#include "Socket.h"
#include "Poller.h"
#include "Typedefs.h"
#include "ananas/util/Buffer.h"

namespace ananas {

namespace internal {
class Acceptor;
class Connector;
}

enum class ShutdownMode {
    eSM_Both,
    eSM_Read,
    eSM_Write,
};

class Connection : public internal::Channel {
public:
    explicit
    Connection(EventLoop* loop);
    ~Connection();

    Connection(const Connection& ) = delete;
    void operator= (const Connection& ) = delete;

    bool Init(int sock, const SocketAddr& peer);

    const SocketAddr& Peer() const {
        return peer_;
    }

    // Active close this connection, will be scheduled later in eventloop.
    void ActiveClose();

    EventLoop* GetLoop() const {
        return loop_;
    }

    // Call ::shutdown at once
    void Shutdown(ShutdownMode mode);

    // NAGLE option
    void SetNodelay(bool enable);

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    // NOT thread-safe
    bool SendPacket(const void* data, std::size_t len);
    bool SendPacket(const std::string& data);
    bool SendPacket(const Buffer& buf);
    bool SendPacket(const BufferVector& datum);
    bool SendPacket(const SliceVector& slice);

    // Thread safe
    bool SafeSend(const void* data, std::size_t len);
    bool SafeSend(const std::string& data);

    // see comment for `batchSend_`, you shouldn't call this func most time.
    void SetBatchSend(bool batch);

    // Callback when connection established.
    void SetOnConnect(std::function<void (Connection* )> cb);
    // Callback when connection disconnected, usually for recycle resourse
    void SetOnDisconnect(std::function<void (Connection* )> cb);
    // Callback when recv data stream.
    void SetOnMessage(TcpMessageCallback cb);
    // Callback when connection disconnected, usually for reconnect
    void SetFailCallback(TcpConnFailCallback cb);

    // Callback when send data without EAGAIN, kernel sendbuffer is enough
    void SetOnWriteComplete(TcpWriteCompleteCallback wccb);
    // Callback when too much data can't send, kernel sendbuffer is full
    void SetOnWriteHighWater(TcpWriteHighWaterCallback whwcb);
    void SetWriteHighWater(size_t s);

    // user context pointer
    void SetUserData(std::shared_ptr<void> user);

    template <typename T>
    std::shared_ptr<T> GetUserData() const;

    // If recv data less than this, do not try onMessage_ callback.
    void SetMinPacketSize(size_t s);
    size_t GetMinPacketSize() const;

private:
    enum State {
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

    // When processing read event, pipeline requests made us handle many
    // requests at one time. If each response is ::send to network directyly,
    // there will be many small packets, lead to poor performance.
    // So if you call SetBatchSend(true), ananas will collect the small packets
    // together, after processing read events, they'll be send all at once.
    // The default value is true. But if your server process only one request at
    // one time, you should call SetBatchSend(false)
    bool processingRead_{false};
    bool batchSend_{true};
    Buffer batchSendBuf_;

    SocketAddr peer_;

    std::function<void (Connection* )> onConnect_;
    std::function<void (Connection* )> onDisconnect_;

    TcpMessageCallback onMessage_;
    TcpConnFailCallback onConnFail_;
    TcpWriteCompleteCallback onWriteComplete_;
    TcpWriteHighWaterCallback onWriteHighWater;

    std::shared_ptr<void> userData_;
};


template <typename T>
inline std::shared_ptr<T> Connection::GetUserData() const {
    return std::static_pointer_cast<T>(userData_);
}

} // end namespace ananas

#endif

