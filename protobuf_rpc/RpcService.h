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


///@file RpcService.h
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

///@brief Rpc Service class, not only wrapper for google::protobuf::Service
class Service final {
public:
    explicit
    Service(GoogleService* service);
    ~Service();

    ///@brief Get the internal google::protobuf::Service, it's
    ///for internal used.
    ///@return The internal google::protobuf::Service
    GoogleService* GetService() const;
    ///@brief Get the name of service
    const std::string& FullName() const;

    ///@brief Set the address to listen
    void SetEndpoint(const Endpoint& ep);
    ///@brief Return the listen address
    const Endpoint& GetEndpoint() const {
        return endpoint_;
    }

    ///@brief Start service, it's called by RpcServer, you shouldn't worry
    bool Start();
    ///@brief Callback for new connection, it's called by library, you shouldn't worry
    void OnNewConnection(Connection* conn);
    void OnRegister();

    ///@brief If the third party protocol, this func tell ananas to invoke which method.
    ///The argument is a pointer to request of type Message, return value is a string,
    ///it should be a valid method name for this service!
    void SetMethodSelector(std::function<std::string (const Message* )> );
    ///@brief Callback when a Rpc ServerChannel created, if you use user-defined protocol,
    /// the callback should call SetEncoder/SetDecoder for this channel.
    void SetOnCreateChannel(std::function<void (ServerChannel* )> );

private:
    friend class ServerChannel;
    friend class HealthServiceImpl;

    static size_t _OnMessage(Connection* conn, const char* data, size_t len);
    void _OnDisconnect(Connection* conn);

    std::unique_ptr<GoogleService> service_;
    Endpoint endpoint_;
    std::string name_;

    std::function<void (ServerChannel* )> onCreateChannel_;
    std::function<std::string (const Message* )> methodSelector_;

    // Each service has many connections, each loop has its own map.
    using ChannelMap = std::unordered_map<unsigned int, ServerChannel* >;
    std::vector<ChannelMap> channels_;
};

///@brief Rpc ServerChannel, wrap for protobuf rpc call.
class ServerChannel {
    friend class Service;
public:
    ServerChannel(Connection* conn, rpc::Service* service);
    ~ServerChannel();

    ///@brief Set user context
    void SetContext(std::shared_ptr<void> ctx);

    ///@brief Get user context
    template <typename T>
    std::shared_ptr<T> GetContext() const;

    ///@brief Get rpc Service which this channel based on
    rpc::Service* Service() const;
    ///@brief Get connection attached by this channel
    Connection* GetConnection() const;

    ///@brief Special encoder for this chanel, see [Encoder](@ref Encoder).
    void SetEncoder(Encoder enc);
    ///@brief Special decoder for this chanel, see [Decoder](@ref Decoder).
    void SetDecoder(Decoder dec);
    ///@brief Called when recv bytes from network, it splits bytes stream
    /// to user packets.
    std::shared_ptr<Message> OnData(const char*& data, size_t len);
    ///@brief Called when OnData return success, req is returned by OnData.
    bool OnMessage(std::shared_ptr<Message>&& req);

private:
    void _Invoke(const std::string& methodName,
                 std::shared_ptr<Message>&& req);
    void _OnServDone(std::weak_ptr<ananas::Connection> wconn,
                     int id,
                     std::shared_ptr<Message> response);
    void _OnError(const std::exception& err, int code = 0);

    ananas::Connection* const conn_;
    rpc::Service* const service_;

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

