
#ifndef BERT_CONNECTOR_H
#define BERT_CONNECTOR_H

#include "Socket.h"
#include "Timer.h"
#include "Typedefs.h"

namespace ananas
{
namespace internal
{

enum class ConnectState
{
    none,
    connecting,
    connected,
    failed,
};

class Connector : public Channel
{
public:
    explicit
    Connector(EventLoop* loop);
    ~Connector();

    Connector(const Connector& ) = delete;
    void operator= (const Connector& ) = delete;

    void SetNewConnCallback(NewTcpConnCallback cb);
    void SetFailCallback(TcpConnFailCallback cb);
    bool Connect(const SocketAddr& addr, DurationMs timeout);

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    ConnectState State() const { return state_; }

private:
    void _OnSuccess();
    void _OnFailed();

    int localSock_ = kInvalid;
    SocketAddr peer_;
    EventLoop* const loop_;

    ConnectState state_ = ConnectState::none;

    TimerId timeoutId_;

    TcpConnFailCallback onConnectFail_;
    NewTcpConnCallback newConnCallback_;
};

} // end namespace internal
} // end namespace ananas

#endif

