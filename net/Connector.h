
#ifndef BERT_CONNECTOR_H
#define BERT_CONNECTOR_H

#include <functional>
#include "Socket.h"
#include "EventLoop.h"

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

class Connector : public EventSource
{
public:
    explicit
    Connector(EventLoop* loop);
    ~Connector();

    Connector(const Connector& ) = delete;
    void operator= (const Connector& ) = delete;

    void SetNewConnCallback(EventLoop::NewConnCallback cb);
    void SetFailCallback(EventLoop::ConnFailCallback cb);
    bool Connect(const SocketAddr& addr);

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


    EventLoop::ConnFailCallback onConnectFail_;
    EventLoop::NewConnCallback newConnCallback_;
};

} // end namespace internal
} // end namespace ananas

#endif

