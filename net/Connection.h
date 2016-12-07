
#ifndef BERT_CONNECTION_H
#define BERT_CONNECTION_H

#include <sys/types.h>
#include "Socket.h"
#include "Poller.h"
#include "Buffer.h"
#include "EventLoop.h"

namespace ananas
{

typedef size_t PacketLen_t;

class Connection : public internal::EventSource
{
public:
    Connection(EventLoop* loop);
    ~Connection();

    Connection(const Connection& ) = delete;
    void operator= (const Connection& ) = delete;

    bool Init(int sock, const SocketAddr& peer);
    void SetMaxPacketSize(std::size_t s);

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    bool SendPacket(const void* data, std::size_t len);

    void SetOnConnect(std::function<void (Connection* )> cb);
    void SetOnDisconnect(std::function<void (Connection* )> cb);
    void SetOnMessage(std::function<PacketLen_t (Connection*, const char* data, PacketLen_t len)> cb);
    void OnConnect();
    void SetFailCallback(EventLoop::ConnFailCallback cb);

private:
    int _Send(const void* data, size_t len);

    EventLoop* loop_;
    int localSock_;
    std::size_t maxPacketSize_;

    Buffer recvBuf_;
    Buffer sendBuf_;

    SocketAddr peer_;

    std::function<void (Connection* )> onConnect_;
    std::function<void (Connection* )> onDisconnect_;
    std::function<PacketLen_t (Connection* , const char* data, PacketLen_t len)> onMessage_;
    
    EventLoop::ConnFailCallback onConnFail_;
};

} // end namespace ananas

#endif

