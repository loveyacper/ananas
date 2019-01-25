#ifndef BERT_TYPEDEFS_H
#define BERT_TYPEDEFS_H

#include <functional>

namespace ananas {

struct SocketAddr;
class Connection;
class DatagramSocket;
class EventLoop;

using NewTcpConnCallback = std::function<void (Connection* )>;
using TcpConnFailCallback = std::function<void (EventLoop*, const SocketAddr& peer)>;
using TcpMessageCallback = std::function<size_t (Connection*, const char* data, size_t len)>;
using TcpWriteCompleteCallback = std::function<void (Connection* )>;
using BindCallback = std::function<void (bool succ, const SocketAddr& )>;

using UDPMessageCallback = std::function<void (DatagramSocket*, const char* data, size_t len)>;
using UDPCreateCallback = std::function<void (DatagramSocket* )>;
}

#endif

