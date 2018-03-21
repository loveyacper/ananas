#ifndef BERT_RPCSERVICESTUB_H
#define BERT_RPCSERVICESTUB_H

#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "net/Typedefs.h"
#include "net/Socket.h"
#include "net/Connection.h"
#include "util/Buffer.h"
#include "future/Future.h"

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
class ClientChannel;
class RpcMessage;


class ServiceStub final
{
public:
    explicit
    ServiceStub(google::protobuf::Service* service);
    ~ServiceStub();

    google::protobuf::Service* GetService() const;
    const std::string& FullName() const;

    // serviceUrl list format : tcp://127.0.0.1:6379;tcp://127.0.0.1:6380
    // TODO name server
    void SetUrlList(const std::string& serviceUrls);

    void SetOnCreateChannel(std::function<void (ClientChannel* )> );

    Future<ClientChannel* > GetChannel();
    Future<ClientChannel* > GetChannel(const Endpoint& ep);

    // encode -->  
    void OnNewConnection(ananas::Connection* conn);

    size_t OnMessage(ananas::Connection* conn, const char* data, size_t len);

    //void SetLoadBalancer(const std::string& serviceUrl);
    void OnRegister();
private:
    Future<ClientChannel* > _Connect(const Endpoint& ep);
    void _OnConnect(ananas::Connection* );
    void _OnDisconnect(ananas::Connection* conn);
    void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer);
    const Endpoint& ChooseOne() const;

    // channels are in different eventLoops
    using ChannelMap = std::unordered_map<Endpoint, ClientChannel* >;
    using ChannelMapPtr = std::shared_ptr<ChannelMap>;
    std::mutex channelMutex_;
    ChannelMapPtr channels_;
    ChannelMapPtr GetChannelMap();

    std::shared_ptr<google::protobuf::Service> service_;

    // pending connects
    using ChannelPromise = Promise<ClientChannel* >;
    std::unordered_map<ananas::SocketAddr, std::vector<ChannelPromise> > pendingConns_;
    // if hardCodedUrls_ is not empty, never access name server
    std::vector<Endpoint> hardCodedUrls_;
    std::string name_;

    // channel init
    std::function<void (ClientChannel* )> onCreateChannel_;
#if 0
    // TODO nameserver & load balance
    std::vector<Endpoint> activeUrls_;
    std::vector<Endpoint> inactiveUrls_;
#endif
};

class ClientChannel
{
    friend class ServiceStub;
public:
    ClientChannel(ananas::Connection* conn, ananas::rpc::ServiceStub* service);
    ~ClientChannel();

    ananas::Connection* Connection() const;

    std::shared_ptr<void> ctx_;
    void SetContext(std::shared_ptr<void> ctx)
    {
        ctx_ = std::move(ctx);
    }

    template <typename T>
    std::shared_ptr<T> GetContext() const
    {
        return std::static_pointer_cast<T>(ctx_);
    }

    template <typename RSP>
    Future<RSP> Invoke(const std::string& method, const ::google::protobuf::Message& request);

    // encode
    ananas::Buffer PbToBytesEncoder(const std::string& method, const google::protobuf::Message& request);
    void SetEncoder(Encoder );
    
    // decode
    std::shared_ptr<google::protobuf::Message> OnData(const char*& data, size_t len);
    // set promise
    bool OnMessage(std::shared_ptr<google::protobuf::Message> msg);

    void SetDecoder(Decoder dec);

private:
    ananas::Connection* const conn_;
    ananas::rpc::ServiceStub* const service_;

    // pending requests
    struct RequestContext {
        Promise<std::shared_ptr<google::protobuf::Message>> promise; 
        std::shared_ptr<google::protobuf::Message> response;
    };

    std::unordered_map<int, RequestContext> pendingCalls_;

    // coders
    Decoder decoder_;
    Encoder encoder_;

    int GenId();
    static thread_local int reqIdGen_;
};

template <typename RSP>
Future<RSP> ClientChannel::Invoke(const std::string& method,
                                  const ::google::protobuf::Message& request)
{
    if (!service_->GetService()->GetDescriptor()->FindMethodByName(method))
        return MakeExceptionFuture<RSP>(std::runtime_error("No such method " + method));

    Promise<std::shared_ptr<google::protobuf::Message>> promise; 
    auto fut = promise.GetFuture();

    // encode and send request
    Buffer bytes = PbToBytesEncoder(method, request);
    if (!conn_->SendPacket(bytes.ReadAddr(), bytes.ReadableSize())) // SendPacketInLoop
    {
        return MakeExceptionFuture<RSP>(std::runtime_error("SendPacket failed"));
    }
    else
    {
        // save context
        RequestContext reqContext;
        reqContext.promise = std::move(promise);
        reqContext.response.reset(new RSP()); // dragon

        // convert RpcMessage to RSP 
        RSP* rsp = (RSP* )reqContext.response.get();
        auto decodeFut = fut.Then([this, rsp](std::shared_ptr<google::protobuf::Message>&& msg) {
                if (decoder_.m2mDecoder_)
                    decoder_.m2mDecoder_(*msg, *rsp);
                else // msg type must be RSP
                    *rsp = std::move(*std::static_pointer_cast<RSP>(msg));

                return std::move(*rsp);
        });
 
        pendingCalls_.insert(std::make_pair(reqIdGen_, std::move(reqContext)));
        return decodeFut;
    }
}


} // end namespace rpc

} // end namespace ananas

#endif

