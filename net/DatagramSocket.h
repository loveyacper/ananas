
#ifndef BERT_DATAGRAMSOCKET_H
#define BERT_DATAGRAMSOCKET_H

#include <list>
#include <functional>
#include "Socket.h"
#include "Poller.h"

namespace ananas
{

class EventLoop;

class DatagramSocket : public internal::EventSource
{
public:
    using MessageCallback = std::function<void (DatagramSocket* , const char* data, size_t len)>;
    using CreateCallback = std::function<void (DatagramSocket* )>;

    explicit
    DatagramSocket(EventLoop* loop);
    ~DatagramSocket();

    DatagramSocket(const DatagramSocket& ) = delete;
    void operator= (const DatagramSocket& ) = delete;

    void SetMaxPacketSize(std::size_t s);
    bool Bind(const SocketAddr* addr);

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    bool SendPacket(const char* , size_t , const SocketAddr* = nullptr);

    const SocketAddr& PeerAddr() const { return srcAddr_; }

    void SetMessageCallback(MessageCallback mcb) { onMessage_ = std::move(mcb); }
    void SetCreateCallback(CreateCallback ccb) { onCreate_ = std::move(ccb); }

private:
    void _PutSendBuf(const char* data, size_t size, const SocketAddr* dst);
    int _Send(const char* data, size_t size, const SocketAddr& dst);

    EventLoop* const loop_;
    int localSock_;
    std::size_t maxPacketSize_;
    SocketAddr srcAddr_;

    struct Package
    {
        SocketAddr dst;
        std::string data;
    };
    std::list<Package> sendList_;
    
    MessageCallback onMessage_;
    CreateCallback onCreate_;
};

} // namespace ananas

#endif

