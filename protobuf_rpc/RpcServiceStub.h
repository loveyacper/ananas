#ifndef BERT_RPCSERVICESTUB_H
#define BERT_RPCSERVICESTUB_H

#include <google/protobuf/message.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "ananas/net/Typedefs.h"
#include "ananas/net/Socket.h"
#include "ananas/net/Connection.h"
#include "ananas/net/EventLoop.h"
#include "ananas/net/AnanasDebug.h"
#include "ananas/util/Buffer.h"
#include "ananas/util/TimeUtil.h"
#include "ananas/util/Timer.h"
#include "ananas/util/StringView.h"
#include "ananas/future/Future.h"

#include "RpcEndpoint.h"
#include "RpcException.h"
#include "ProtobufCoder.h"

namespace google {
namespace protobuf {

class Service;

}
}

///@file RpcServiceStub.h
namespace ananas {

namespace rpc {

class Request;
class ClientChannel;
class RpcMessage;

using google::protobuf::Message;
using GoogleService = google::protobuf::Service;

///@brief Rpc ServiceStub class.
/// It's used for rpc client side inside rpc library.
/// You needn't call rpc with it directly.
/// But you should create service stub before main loop start,
/// Set the service name that you interested in, name server or
/// address list of service, the channel initializer.
class ServiceStub final {
public:
    explicit
    ServiceStub(GoogleService* service);
    ~ServiceStub();

    GoogleService* GetService() const;
    const std::string& FullName() const;

    ///@brief Set the service address
    ///
    /// Format : tcp://127.0.0.1:6379;tcp://127.0.0.1:6380
    /// If you call this function, ananas will made rpc call directly connect
    /// to these addressed, not via name service
    void SetUrlList(const std::string& hardCodedUrls);
    void SetOnCreateChannel(std::function<void (ClientChannel* )> );

    ///@brief Get channel by some load balance.
    ///
    /// It's for internal use, you should not call these function
    Future<ClientChannel* > GetChannel();
    Future<ClientChannel* > GetChannel(const Endpoint& ep);

    void OnRegister();
private:
    Future<ClientChannel* > _Connect(EventLoop*, const Endpoint& ep);

    void _OnNewConnection(Connection* );
    void _OnConnect(Connection* );
    void _OnDisconnect(Connection* );
    void _OnConnFail(EventLoop* loop, const SocketAddr& peer);
    static size_t _OnMessage(Connection* conn, const char* data, size_t len);

    using EndpointsPtr = std::shared_ptr<std::vector<Endpoint>>;

    // May visit name server for service endpoints, so return future type
    Future<EndpointsPtr> _GetEndpoints();

    // Call _SelectEndpoint and _MakeChannel
    Future<ClientChannel* > _SelectChannel(EventLoop*, Try<EndpointsPtr>&& );

    // Try establish connection with endpoint
    Future<ClientChannel* > _MakeChannel(EventLoop*, const Endpoint& );

    // Choose a endpoint by some load balance algorithm
    static const Endpoint& _SelectEndpoint(EndpointsPtr );

    // Callback for nameserver's response
    void _OnNewEndpointList(Try<EndpointList>&& );

    using ChannelMap = std::unordered_map<Endpoint, std::shared_ptr<ClientChannel> >;
    std::vector<ChannelMap> channels_;

    std::shared_ptr<GoogleService> service_;

    // pending connects
    using ChannelPromise = Promise<ClientChannel* >;
    // one unordered_map per loop
    std::vector<std::unordered_map<SocketAddr, std::vector<ChannelPromise>>> pendingConns_;
    // if hardCodedUrls_ is not empty, never access name server
    std::shared_ptr<std::vector<Endpoint>> hardCodedUrls_;
    std::string name_;

    // channel init
    std::function<void (ClientChannel* )> onCreateChannel_;

    // active nodes fetched from name server
    std::mutex endpointsMutex_;

    EndpointsPtr endpoints_;
    std::vector<Promise<EndpointsPtr>> pendingEndpoints_;
    Time refreshTime_;
};

///@brief Rpc ClientChannel, wrap for protobuf rpc call.
class ClientChannel {
    friend class ServiceStub;
public:
    ClientChannel(std::shared_ptr<Connection>&& conn, ServiceStub* service);
    ~ClientChannel();

    ///@brief Set user context
    void SetContext(std::shared_ptr<void> ctx);

    ///@brief Get user context
    template <typename T>
    std::shared_ptr<T> GetContext() const;

    ///@brief Call method by request
    ///@return A future that can be register callback or timeout
    ///
    /// It's used for rpc library internal.
    template <typename RSP>
    Future<Try<RSP>> Invoke(const StringView& method,
                            const std::shared_ptr<Message>& request);

    ///@brief Special encoder for this chanel, see [Encoder](@ref Encoder).
    void SetEncoder(Encoder );

    ///@brief Called when recv bytes from network, it splits bytes stream
    /// to user packets.
    std::shared_ptr<Message> OnData(const char*& data, size_t len);
    ///@brief Special decoder for this chanel, see [Decoder](@ref Decoder).
    void SetDecoder(Decoder dec);

    ///@brief Called when OnData return success, req is returned by OnData.
    ///
    /// It will call SetValue on promise, so trigger your Then callback when you
    /// made rpc::Call.
    bool OnMessage(std::shared_ptr<Message> msg);

    ///@brief Used by rpc library internal.
    void OnDestroy();

private:
    template <typename RSP>
    Future<Try<RSP>> _Invoke(const StringView& method,
                             const std::shared_ptr<Message>& request);

    void _CheckPendingTimeout();
    std::weak_ptr<Connection> conn_;
    ServiceStub* const service_;

    std::shared_ptr<void> ctx_;

    Buffer _MessageToBytesEncoder(std::string&& method, const Message& request);

    // pending requests
    struct RequestContext {
        Promise<std::shared_ptr<Message>> promise;
        std::shared_ptr<Message> response;
        Time timestamp;
    };

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
std::shared_ptr<T> ClientChannel::GetContext() const {
    return std::static_pointer_cast<T>(ctx_);
}

template <typename RSP>
Future<Try<RSP>> ClientChannel::Invoke(const StringView& method,
                                       const std::shared_ptr<Message>& request) {
    auto sc = conn_.lock();
    if (!sc) {
        // When we're ready to send packet, the connection lost :-(
        std::string err("Connection lost: method [" +
                         method.ToString() +
                         "], service [" +
                         service_->FullName() + "]");
        return MakeExceptionFuture<Try<RSP>>(Exception(ErrorCode::ConnectionLost, err));
    }

    if (sc->GetLoop()->InThisLoop()) {
        return _Invoke<RSP>(method, request);
    } else {
        auto invoker(std::bind(&ClientChannel::_Invoke<RSP>, this, method, request));
        return sc->GetLoop()->Execute(std::move(invoker)).Unwrap();
    }
}


template <typename RSP>
Future<Try<RSP>> ClientChannel::_Invoke(const StringView& method,
                                        const std::shared_ptr<Message>& request) {
    auto sc = conn_.lock();
    assert (sc);
    assert (sc->GetLoop()->InThisLoop());

    auto methodStr = method.ToString();
    if (!service_->GetService()->GetDescriptor()->FindMethodByName(methodStr)) {
        std::string err("method [" +
                         methodStr +
                        "], service [" +
                         service_->FullName() + "]");
        return MakeExceptionFuture<Try<RSP>>(Exception(ErrorCode::NoSuchMethod, err));
    }

    Promise<std::shared_ptr<Message>> promise;
    auto fut = promise.GetFuture();

    // encode and send request
    Buffer bytes = this->_MessageToBytesEncoder(std::move(methodStr), *request);
    if (!sc->SendPacket(bytes)) {
        using namespace std;
        string err("SendPacket failed: method [" +
                    method.ToString() +
                   "], service [" +
                   service_->FullName() + "]");
        return MakeExceptionFuture<Try<RSP>>(Exception(ErrorCode::ConnectionReset, err));
    } else {
        // save context
        RequestContext reqContext;
        reqContext.promise = std::move(promise);
        reqContext.response.reset(new RSP()); // It's delicious :-)

        // convert RpcMessage to RSP
        RSP* rsp = (RSP* )reqContext.response.get();
        auto decodeF = fut.Then([this, rsp](std::shared_ptr<Message>&& msg) -> Try<RSP> {
            // Convert to user message type
            if (decoder_.m2mDecoder_) {
                try {
                    decoder_.m2mDecoder_(*msg, *rsp);
                } catch (const std::exception& exp) {
                    return Try<RSP>(std::current_exception());
                } catch (...) {
                    ANANAS_ERR << "Unknown exception when m2mDecode";
                    return Try<RSP>(std::current_exception());
                }
            } else {
                // msg type must be of RSP type
                *rsp = std::move(*std::static_pointer_cast<RSP>(msg));
            }

            return std::move(*rsp);
        });

        // saving request context
        pendingCalls_.insert(std::make_pair(reqIdGen_, std::move(reqContext)));
        return decodeF;
    }
}

} // end namespace rpc

} // end namespace ananas

#endif

