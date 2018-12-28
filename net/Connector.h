
#ifndef BERT_CONNECTOR_H
#define BERT_CONNECTOR_H

#include "Socket.h"
#include "Typedefs.h"
#include "ananas/util/Timer.h"

namespace ananas {
namespace internal {

enum class ConnectState {
    none,
    connecting,
    connected,
    failed,
};

class Connector : public Channel {
public:
    explicit
    Connector(EventLoop* loop);
    ~Connector();

    Connector(const Connector& ) = delete;
    void operator= (const Connector& ) = delete;

    // Callback when connect success as new connection
    void SetNewConnCallback(NewTcpConnCallback cb);
    // Callback when connect failed, usually retry
    void SetFailCallback(TcpConnFailCallback cb);

    // Connect with timeout, if EventLoop is not null, new connection
    // will be put into it.
    bool Connect(const SocketAddr& , DurationMs , EventLoop* = nullptr);

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

private:
    void _OnSuccess();
    void _OnFailed();

    int localSock_ = kInvalid;
    SocketAddr peer_;
    EventLoop* const loop_;
    EventLoop* dstLoop_;

    ConnectState state_ = ConnectState::none;

    TimerId timeoutId_;

    TcpConnFailCallback onConnectFail_;
    NewTcpConnCallback newConnCallback_;
};

} // end namespace internal
} // end namespace ananas

#endif

