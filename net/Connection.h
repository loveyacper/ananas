
#ifndef BERT_CONNECTION_H
#define BERT_CONNECTION_H

#include <sys/types.h>
#include <string>

#include "Socket.h"
#include "Poller.h"
#include "Typedefs.h"
#include "ananas/util/Buffer.h"

///@file Connection.h
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

///@brief Abstract for stream socket
class Connection : public internal::Channel {
public:
    explicit
    Connection(EventLoop* loop);
    ~Connection();

    Connection(const Connection& ) = delete;
    void operator= (const Connection& ) = delete;

    ///@brief Init, called by library internal
    bool Init(int sock, const SocketAddr& peer);

    ///@brief Got peer address
    ///@return Peer address, you can print it with ToString() method.
    const SocketAddr& Peer() const {
        return peer_;
    }

    ///@brief Active close this connection, will be scheduled later in eventloop.
    void ActiveClose();

    ///@brief Return pointer to EventLoop which this connection in.
    EventLoop* GetLoop() const {
        return loop_;
    }

    ///@brief Call ::shutdown at once
    void Shutdown(ShutdownMode mode);

    ///@brief About the NAGLE option
    void SetNodelay(bool enable);

    ///@brief Return socket fd
    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    ///@brief Send bytes to network
    ///
    /// NOT thread-safe
    bool SendPacket(const void* data, std::size_t len);
    ///@brief Send bytes to network
    ///
    /// NOT thread-safe
    bool SendPacket(const std::string& data);
    ///@brief Send bytes to network
    ///
    /// NOT thread-safe
    bool SendPacket(const Buffer& buf);
    ///@brief Send vector bytes to network
    ///
    /// NOT thread-safe
    bool SendPacket(const BufferVector& datum);
    ///@brief Send vector bytes to network
    ///
    /// NOT thread-safe
    bool SendPacket(const SliceVector& slice);

    ///@brief Send bytes to network
    ///
    /// Thread-safe
    bool SafeSend(const void* data, std::size_t len);
    bool SafeSend(const std::string& data);

    ///@brief Something internal.
    ///
    ///    When processing read event, pipeline requests made us handle many
    ///    requests at one time. If each response is ::send to network directyly,
    ///    there will be many small packets, lead to poor performance.
    ///    So if you call SetBatchSend(true), ananas will collect the small packets
    ///    together, after processing read events, they'll be send all at once.
    ///    The default value is true. But if your server process only one request at
    ///    one time, you should call SetBatchSend(false)
    void SetBatchSend(bool batch);

    ///@brief Callback when connection established.
    void SetOnConnect(std::function<void (Connection* )> cb);
    ///@brief Callback when connection disconnected, usually for recycle resourse
    void SetOnDisconnect(std::function<void (Connection* )> cb);
    ///@brief Callback when recv data stream.
    void SetOnMessage(TcpMessageCallback cb);
    ///@brief Callback when send data without EAGAIN, kernel sendbuffer is enough
    void SetOnWriteComplete(TcpWriteCompleteCallback wccb);

    ///@brief Set user's context pointer
    void SetUserData(std::shared_ptr<void> user);

    ///@brief Get user's context pointer
    template <typename T>
    std::shared_ptr<T> GetUserData() const;

    ///@brief Set the min size of your business packet
    ///
    /// If recv data less than this, never try onMessage_ callback.
    void SetMinPacketSize(size_t s);
    ///@brief Get the min size of your business packet
    size_t GetMinPacketSize() const;

private:
    enum State {
        eS_None,
        eS_Connected,
        eS_CloseWaitWrite,  // passive or active close, but I still have data to send
        eS_PassiveClose,    // should close
        eS_ActiveClose,     // should close
        eS_Error,           // waiting to handle error
        eS_Closed,          // final state being to destroy
    };

    friend class internal::Acceptor;
    friend class internal::Connector;
    void _OnConnect();
    int _Send(const void* data, size_t len);

    EventLoop* const loop_;
    State state_ = State::eS_None;
    int localSock_;
    size_t minPacketSize_;

    Buffer recvBuf_;
    BufferVector sendBuf_;

    bool processingRead_{false};
    bool batchSend_{true};
    Buffer batchSendBuf_;

    SocketAddr peer_;

    std::function<void (Connection* )> onConnect_;
    std::function<void (Connection* )> onDisconnect_;

    TcpMessageCallback onMessage_;
    TcpWriteCompleteCallback onWriteComplete_;

    std::shared_ptr<void> userData_;
};


template <typename T>
inline std::shared_ptr<T> Connection::GetUserData() const {
    return std::static_pointer_cast<T>(userData_);
}

} // end namespace ananas

#endif

