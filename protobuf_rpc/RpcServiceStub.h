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
#include "net/EventLoop.h"
#include "net/AnanasDebug.h"
#include "util/Buffer.h"
#include "util/TimeUtil.h"
#include "util/Timer.h"
#include "util/StringView.h"
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
    // Direct connect to, not via name service
    void SetUrlList(const std::string& hardCodedUrls);
    void SetOnCreateChannel(std::function<void (ClientChannel* )> );

    Future<ClientChannel* > GetChannel();
    Future<ClientChannel* > GetChannel(const Endpoint& ep);

    void OnRegister();
private:
    Future<ClientChannel* > _Connect(EventLoop* , const Endpoint& ep);

    void _OnNewConnection(Connection* );
    void _OnConnect(Connection* );
    void _OnDisconnect(Connection* );
    void _OnConnFail(EventLoop* loop, const SocketAddr& peer);
    static size_t _OnMessage(Connection* conn, const char* data, size_t len);

    Future<std::shared_ptr<std::vector<Endpoint>>> _GetEndpoints();
    Future<ClientChannel* > _SelectChannel(EventLoop* , Try<std::shared_ptr<std::vector<Endpoint>>>&& );
    Future<ClientChannel* > _MakeChannel(EventLoop* , const Endpoint& );
    static const Endpoint& _SelectEndpoint(std::shared_ptr<std::vector<Endpoint>> );
    void _OnNewEndpointList(Try<EndpointList>&& );

    using ChannelMap = std::unordered_map<Endpoint, std::shared_ptr<ClientChannel> >;
    std::vector<ChannelMap> channels_;

    std::shared_ptr<google::protobuf::Service> service_;

    // pending connects
    using ChannelPromise = Promise<ClientChannel* >;
    // one unordered_map per loop
    std::vector<std::unordered_map<SocketAddr, std::vector<ChannelPromise> >> pendingConns_;
    // if hardCodedUrls_ is not empty, never access name server
    std::shared_ptr<std::vector<Endpoint>> hardCodedUrls_;
    std::string name_;

    // channel init
    std::function<void (ClientChannel* )> onCreateChannel_;

    // active nodes fetched from name server
    std::mutex endpointsMutex_;

    using EndpointsPtr = std::shared_ptr<std::vector<Endpoint>>;
    EndpointsPtr endpoints_;
    std::vector<Promise<EndpointsPtr>> pendingEndpoints_;
    //ananas::Time refreshTime_; // TODO refresh endpoints
};

class ClientChannel
{
    friend class ServiceStub;
public:
    ClientChannel(std::shared_ptr<Connection> conn, ServiceStub* service);
    ~ClientChannel();

    void SetContext(std::shared_ptr<void> ctx);

    template <typename T>
    std::shared_ptr<T> GetContext() const;

    template <typename RSP>
    Future<Try<RSP>> Invoke(const ananas::StringView& method,
                            const std::shared_ptr<google::protobuf::Message>& request);

    // encode
    void SetEncoder(Encoder );
    
    // decode
    std::shared_ptr<google::protobuf::Message> OnData(const char*& data, size_t len);
    void SetDecoder(Decoder dec);

    // fullfil promise
    bool OnMessage(std::shared_ptr<google::protobuf::Message> msg);

    void OnDestroy();

private:
    template <typename RSP>
    Future<Try<RSP>> _Invoke(const ananas::StringView& method,
                             const std::shared_ptr<google::protobuf::Message>& request);

    void _CheckPendingTimeout();
    std::weak_ptr<ananas::Connection> conn_;
    ServiceStub* const service_;

    std::shared_ptr<void> ctx_;

    Buffer _MessageToBytesEncoder(std::string&& method, const google::protobuf::Message& request);

    // pending requests
    struct RequestContext {
        Promise<std::shared_ptr<google::protobuf::Message>> promise; 
        std::shared_ptr<google::protobuf::Message> response;
        ananas::Time timestamp;
    };

    // TODO
    std::map<int, RequestContext> pendingCalls_;
    // Check Timeout ID
    TimerId pendingTimeoutId_;

    // coders
    Decoder decoder_;
    Encoder encoder_;

    int GenId();
    static thread_local int reqIdGen_;
};

template <typename T>
std::shared_ptr<T> ClientChannel::GetContext() const
{
    return std::static_pointer_cast<T>(ctx_);
}

template <typename RSP>
Future<Try<RSP>> ClientChannel::Invoke(const ananas::StringView& method,
                                       const std::shared_ptr<google::protobuf::Message>& request)
{
    auto sc = conn_.lock();
    if (!sc)
        return MakeExceptionFuture<Try<RSP>>(std::runtime_error("Connection lost when invoke [" +
                                                                 method.ToString() +
                                                                 "]"));

    if (sc->GetLoop()->IsInSameLoop())
    {
        return _Invoke<RSP>(method, request);
    }
    else
    {
        auto invoker = std::bind(&ClientChannel::_Invoke<RSP>, this, method, request);
        return sc->GetLoop()->Execute(std::move(invoker)).Unwrap();
    }
}


template <typename RSP>
Future<Try<RSP>> ClientChannel::_Invoke(const ananas::StringView& method,
                                        const std::shared_ptr<google::protobuf::Message>& request)
{
    auto sc = conn_.lock();
    assert (sc);
    assert (sc->GetLoop()->IsInSameLoop());

    auto methodStr = method.ToString();
    if (!service_->GetService()->GetDescriptor()->FindMethodByName(methodStr)) 
        return MakeExceptionFuture<Try<RSP>>(std::runtime_error("No such method [" + methodStr + "]"));

    Promise<std::shared_ptr<google::protobuf::Message>> promise; 
    auto fut = promise.GetFuture();

    // encode and send request
    Buffer bytes = this->_MessageToBytesEncoder(std::move(methodStr), *request);
    if (!sc->SendPacket(bytes))
    {
        return MakeExceptionFuture<Try<RSP>>(std::runtime_error("SendPacket failed!"));
    }
    else
    {
        // save context
        RequestContext reqContext;
        reqContext.promise = std::move(promise);
        reqContext.response.reset(new RSP()); // here be dragon

        // convert RpcMessage to RSP 
        RSP* rsp = (RSP* )reqContext.response.get();
        auto decodeFut = fut.Then([this, rsp](std::shared_ptr<google::protobuf::Message>&& msg) -> Try<RSP> {
                if (decoder_.m2mDecoder_) {
                    try {
                        decoder_.m2mDecoder_(*msg, *rsp);
                    }
                    catch (const std::exception& exp) {
                        return Try<RSP>(std::current_exception());
                    }
                    catch (...) {
                        ANANAS_ERR << "Unknown exception when m2mDecode";
                        return Try<RSP>(std::current_exception());
                    }
                }
                else
                    // msg type must be RSP
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

