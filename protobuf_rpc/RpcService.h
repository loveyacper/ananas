#ifndef BERT_RPCSERVICE_H
#define BERT_RPCSERVICE_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "net/Typedefs.h"
#include "net/Socket.h"

#include "RpcEndpoint.h"
#include "ProtobufCoder.h"


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

    void SetEndpoint(const Endpoint& ep);
    const Endpoint& GetEndpoint() const { return endpoint_; }
    bool Start();

    void OnNewConnection(ananas::Connection* conn);

    void OnRegister();
    
    // if the third party protocol, this func tell ananas to invoke which func for request
    void SetMethodSelector(std::function<std::string (const google::protobuf::Message* )> );

    void SetOnCreateChannel(std::function<void (ServerChannel* )> );

private:
    friend class ServerChannel;

    static size_t _OnMessage(ananas::Connection* conn, const char* data, size_t len);
    void _OnDisconnect(ananas::Connection* conn);

    std::unique_ptr<google::protobuf::Service> service_;
    Endpoint endpoint_;
    std::string name_;

    std::function<void (ServerChannel* )> onCreateChannel_;
    std::function<std::string (const google::protobuf::Message* )> methodSelector_;

    // each service has many connections, each loop has its own map, avoid mutex
    using ChannelMap = std::unordered_map<unsigned int, ServerChannel* >;
    std::vector<ChannelMap> channels_;
};

class ServerChannel
{
    friend class Service;
public:
    ServerChannel(ananas::Connection* conn, ananas::rpc::Service* service);
    ~ServerChannel();

    ananas::rpc::Service* Service() const;
    ananas::Connection* Connection() const;

    void SetEncoder(Encoder enc);

    void SetDecoder(Decoder dec);
    std::shared_ptr<google::protobuf::Message> OnData(const char*& data, size_t len);
    bool OnMessage(std::shared_ptr<google::protobuf::Message> req);
    void OnError(const std::exception& err);
private:
    void _Invoke(const std::string& methodName,
                std::shared_ptr<google::protobuf::Message> req);
    void _OnServDone(std::weak_ptr<ananas::Connection> wconn,
                     int id,
                     std::shared_ptr<google::protobuf::Message> response);

    ananas::Connection* const conn_;
    ananas::rpc::Service* const service_;

    // coders
    Decoder decoder_;
    Encoder encoder_;

    int currentId_ {0};
};

} // end namespace rpc

} // end namespace ananas

#endif

