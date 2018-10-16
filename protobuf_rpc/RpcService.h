#ifndef BERT_RPCSERVICE_H
#define BERT_RPCSERVICE_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "ananas/net/Typedefs.h"
#include "ananas/net/Socket.h"

#include "RpcEndpoint.h"
#include "ProtobufCoder.h"


namespace google {
namespace protobuf {

class Message;
class Service;

}
}

namespace ananas {

class Connection;

namespace rpc {

class Request;
class ServerChannel;

using google::protobuf::Message;
using GoogleService = google::protobuf::Service;

class Service final {
public:
    explicit
    Service(GoogleService* service);
    ~Service();

    GoogleService* GetService() const;
    const std::string& FullName() const;

    void SetEndpoint(const Endpoint& ep);
    const Endpoint& GetEndpoint() const {
        return endpoint_;
    }

    bool Start();
    void OnNewConnection(Connection* conn);
    void OnRegister();

    // if the third party protocol, this func tell ananas to invoke which method
    void SetMethodSelector(std::function<std::string (const Message* )> );
    void SetOnCreateChannel(std::function<void (ServerChannel* )> );

private:
    friend class ServerChannel;
    friend class HealthServiceImpl;

    static size_t _OnMessage(ananas::Connection* conn, const char* data, size_t len);
    void _OnDisconnect(ananas::Connection* conn);

    std::unique_ptr<GoogleService> service_;
    Endpoint endpoint_;
    std::string name_;

    std::function<void (ServerChannel* )> onCreateChannel_;
    std::function<std::string (const Message* )> methodSelector_;

    // Each service has many connections, each loop has its own map.
    using ChannelMap = std::unordered_map<unsigned int, ServerChannel* >;
    std::vector<ChannelMap> channels_;
};

class ServerChannel {
public:
    ServerChannel(ananas::Connection* conn, ananas::rpc::Service* service);
    ~ServerChannel();

    void SetContext(std::shared_ptr<void> ctx);

    template <typename T>
    std::shared_ptr<T> GetContext() const;

    ananas::rpc::Service* Service() const;
    ananas::Connection* Connection() const;

    void SetEncoder(Encoder enc);

    void SetDecoder(Decoder dec);
    std::shared_ptr<Message> OnData(const char*& data, size_t len);
    bool OnMessage(std::shared_ptr<Message> req);
    void OnError(const std::exception& err, int code = 0);

private:
    void _Invoke(const std::string& methodName,
                 std::shared_ptr<Message> req);
    void _OnServDone(std::weak_ptr<ananas::Connection> wconn,
                     int id,
                     std::shared_ptr<Message> response);

    ananas::Connection* const conn_;
    ananas::rpc::Service* const service_;

    std::shared_ptr<void> ctx_;

    // coders
    Decoder decoder_;
    Encoder encoder_;

    int currentId_ {0};
};

template <typename T>
std::shared_ptr<T> ServerChannel::GetContext() const {
    return std::static_pointer_cast<T>(ctx_);
}


} // end namespace rpc

} // end namespace ananas

#endif

