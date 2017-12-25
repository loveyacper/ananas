#ifndef BERT_TYPEDEFS_H
#define BERT_TYPEDEFS_H

#include <functional>

namespace ananas
{
    typedef size_t PacketLen_t;

    struct SocketAddr;
    class Connection;
    class DatagramSocket;
    class EventLoop;

    using NewTcpConnCallback = std::function<void (Connection* )>;
    using TcpConnFailCallback = std::function<void (EventLoop*, const SocketAddr& peer)>;
    using TcpMessageCallback = std::function<PacketLen_t (Connection* , const char* data, PacketLen_t len)>;
    using TcpWriteCompleteCallback = std::function<void (Connection* )>;
    using TcpWriteHighWaterCallback = std::function<void (Connection* , size_t toSend)>;
    using BindFailCallback = std::function<void (bool succ, const SocketAddr& )>;

    using UDPMessageCallback = std::function<void (DatagramSocket* , const char* data, size_t len)>;
    using UDPCreateCallback = std::function<void (DatagramSocket* )>;

    using SocketPairCreateCallback = std::function<void (Connection* r, Connection* w)>;
}

#endif

