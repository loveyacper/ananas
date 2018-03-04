#ifndef BERT_RPCSERVICE_H
#define BERT_RPCSERVICE_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "net/Typedefs.h"
#include "net/Socket.h"


namespace google
{
namespace protobuf
{

class Message;
class Service;

}
}

namespace ananas
{

class Connection;

namespace rpc
{

class Request;
class ServerChannel;

class Service final
{
public:
    explicit
    Service(google::protobuf::Service* service);
    ~Service();

    google::protobuf::Service* GetService() const;
    const std::string& FullName() const;

    void SetBindAddr(const SocketAddr& addr);
    bool Start();

    void SetOnMessage(TcpMessageCallback cb);
    void OnNewConnection(ananas::Connection* conn);

    void OnRegister();
private:
    void _OnDisconnect(ananas::Connection* conn);

    // each service has many connections
    // each evloop has its map, avoid mutex
    using ChannelMap = std::unordered_map<unsigned int, ServerChannel* >;
    std::vector<ChannelMap> channels_;

    std::unique_ptr<google::protobuf::Service> service_;
    TcpMessageCallback handleMsg_;
    SocketAddr bindAddr_;
    std::string name_;

    int minLen_;

public:
    size_t OnProtobufMessage(ananas::Connection* conn, const char* data, size_t len);
    void _ProcessRequest(ananas::Connection* , const ananas::rpc::Request& );
    void _OnServDone(std::weak_ptr<ananas::Connection> conn,
                     int id,
                     std::shared_ptr<google::protobuf::Message> response);
    void _OnServError(ananas::Connection* conn,
                      int id,
                      int errno,
                      const std::string& errMsg);
};

class ServerChannel
{
    friend class Service;
public:
    ServerChannel(ananas::Connection* conn, ananas::rpc::Service* service);
    ~ServerChannel();

    ananas::rpc::Service* Service() const;
    ananas::Connection* Connection() const;

private:
    ananas::Connection* const conn_;
    ananas::rpc::Service* const service_;
};

} // end namespace rpc

} // end namespace ananas

#endif

