
#ifndef BERT_ACCEPTOR_H
#define BERT_ACCEPTOR_H

#include "Socket.h"
#include "Typedefs.h"

namespace ananas
{
namespace internal
{

class Acceptor : public Channel
{
public:
    explicit
    Acceptor(EventLoop* loop);
    ~Acceptor();
    
    Acceptor(const Acceptor& ) = delete;
    void operator= (const Acceptor& ) = delete;

    void SetNewConnCallback(NewTcpConnCallback cb);
    bool Bind(const SocketAddr& addr);
        
    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

private:
    int _Accept();

    SocketAddr peer_;
    int localSock_;
    uint16_t localPort_;

    EventLoop* const loop_; // which loop belong to

    //register msg callback and on connect callback for conn
    NewTcpConnCallback newConnCallback_;

    static const int kListenQueue;
};

} // end namespace internal
} // end namespace ananas

#endif

