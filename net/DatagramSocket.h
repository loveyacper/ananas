
#ifndef BERT_DATAGRAMSOCKET_H
#define BERT_DATAGRAMSOCKET_H

#include <list>
#include "Socket.h"
#include "Typedefs.h"
#include "Poller.h"

namespace ananas {

class EventLoop;

class DatagramSocket : public internal::Channel {
public:
    explicit
    DatagramSocket(EventLoop* loop);
    ~DatagramSocket();

    DatagramSocket(const DatagramSocket& ) = delete;
    void operator= (const DatagramSocket& ) = delete;

    void SetMaxPacketSize(std::size_t s);
    std::size_t GetMaxPacketSize() const { return maxPacketSize_; }
    bool Bind(const SocketAddr* addr);

    int Identifier() const override;
    bool HandleReadEvent() override;
    bool HandleWriteEvent() override;
    void HandleErrorEvent() override;

    bool SendPacket(const void*, size_t, const SocketAddr* = nullptr);

    const SocketAddr& PeerAddr() const {
        return srcAddr_;
    }

    void SetMessageCallback(UDPMessageCallback mcb) {
        onMessage_ = std::move(mcb);
    }
    void SetCreateCallback(UDPCreateCallback ccb) {
        onCreate_ = std::move(ccb);
    }

private:
    void _PutSendBuf(const void* data, size_t size, const SocketAddr* dst);
    int _Send(const void* data, size_t size, const SocketAddr& dst);

    EventLoop* const loop_;
    int localSock_;
    std::size_t maxPacketSize_;
    SocketAddr srcAddr_;

    struct Package {
        SocketAddr dst;
        std::string data;
    };
    std::list<Package> sendList_;

    UDPMessageCallback onMessage_;
    UDPCreateCallback onCreate_;
};

} // namespace ananas

#endif

