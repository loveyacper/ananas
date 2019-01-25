#ifndef BERT_RPCSERVER_H
#define BERT_RPCSERVER_H

#include <google/protobuf/message.h>

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "RpcEndpoint.h"
#include "RpcServiceStub.h"
#include "RpcException.h"
#include "ananas/util/StringView.h"
#include "ananas/future/Future.h"

///@file RpcServer.h
namespace ananas {

class Application;

///@brief Sub namespace rpc in namespace ananas.
namespace rpc {

using google::protobuf::Message;

class Service;
class ServiceStub;

///@brief Rpc Server class.
/// It should be singleton.
/// It's just a skeleton class, manage the event loop threads
/// and services & service stubs.
class Server {
public:
    Server();
    static Server& Instance();

    ///@brief Get default event loop
    ///
    /// The base event loop. It's created by default.
    /// If you never call [SetNumOfWorker](@ref SetNumOfWorker), base loop
    /// will be the only event loop, working for listen&connect
    /// and network io. But if you have called SetNumOfWorker, base
    /// loop will only work for listen & connect, real network io will
    /// happen in other event loops.
    ///
    ///@return pointer to default event loop
    EventLoop* BaseLoop();

    ///@brief Get event loop with round robin
    ///@return Pointer to event loop
    EventLoop* Next();

    ///@brief Set init func before running of event loop
    ///@param init Init function
    void SetOnInit(std::function<bool (int, char*[])> init);

    ///@brief Set exit func after running of event loop
    ///@param onExit Exit function
    void SetOnExit(std::function<void ()> onExit);

    ///@brief Name server for service register and discovery.
    ///@param url The url for name server, like "tcp://127.0.0.1:6379",
    /// multi addresses are sperated by comma.
    void SetNameServer(const std::string& url);

    ///@brief Set callback for init name server channel.
    ///@param cb The default is `OnCreateRedisChannel`, you could use redis as
    /// name server by default.
    void SetOnCreateNameServerChannel(std::function<void (ClientChannel*)> cb);

    ///@brief Health service, you can visit this url by your favorite web brower.
    void SetHealthService(const std::string& url);

    ///@brief The server side.
    bool AddService(Service* service);
    bool AddService(std::unique_ptr<Service>&& service);

    ///@brief Stub for the client side.
    ///@param service It's returned by new, will transfer ownership to ananas.
    bool AddServiceStub(ServiceStub* service);

    ///@brief Stub for the client side.
    ///@param service It's returned by make_unique, will transfer ownership to ananas.
    bool AddServiceStub(std::unique_ptr<ServiceStub>&& service);

    ///@brief Get service stub by name. It's used by library internal, so
    /// you needn't worry about it. It needn't to be thread-safe, because
    /// we never add stubs during runtime.
    ServiceStub* GetServiceStub(const StringView& name) const;

    ///@brief Set work threads
    void SetNumOfWorker(size_t n);
    size_t NumOfWorker() const;

    ///@brief Start the rpc server, it should be called after AddService, AddServiceStub
    void Start(int argc, char* argv[]);

    ///@brief Stop the rpc server
    void Shutdown();

private:
    friend class HealthServiceImpl;

    Application& app_;
    std::unordered_map<StringView, std::unique_ptr<Service> > services_;
    std::unordered_map<StringView, std::unique_ptr<ServiceStub> > stubs_;

    ServiceStub* nameServiceStub_ {nullptr};
    Service* healthService_ {nullptr};
    std::function<void (ClientChannel*)> onCreateNameServiceChannel_;

    // The services's info
    std::vector<KeepaliveInfo> keepaliveInfo_;

    static Server* s_rpcServer;
};


#define RPC_SERVER ::ananas::rpc::Server::Instance()

namespace {

// internal use
template <typename RSP>
Future<Try<RSP>> _InnerCall(ServiceStub* stub,
                            const StringView& method,
                            const std::shared_ptr<Message>& reqCopy,
                            const Endpoint& ep = Endpoint::default_instance());
} // end namespace


///@brief The entry point for initiate a rpc call.
///@param service The full name of a service, eg. "test.videoservice"
///@param method One of the `service`'s method's name, eg. "changeVideoQuality"
///@param reqCopy The request self
///@param ep The server address, it's optional if you have name server
///@return A future for rpc call. `RSP` is the type of response. why use `Try` type? Because may throw exception value.

template <typename RSP>
Future<Try<RSP>> Call(const StringView& service,
                      const StringView& method,
                      const std::shared_ptr<Message>& reqCopy,
                      const Endpoint& ep = Endpoint::default_instance()) {
    // 1. find service stub
    auto stub = RPC_SERVER.GetServiceStub(service);
    if (!stub)
        return MakeExceptionFuture<Try<RSP>>(Exception(ErrorCode::NoSuchService, service.ToString()));

    return _InnerCall<RSP>(stub, method, reqCopy, ep);
}

template <typename RSP>
Future<Try<RSP>> Call(const StringView& service,
                      const StringView& method,
                      const Message& req,
                      const Endpoint& ep = Endpoint::default_instance()) {
    // 1. find service stub
    auto stub = RPC_SERVER.GetServiceStub(service);
    if (!stub)
        return MakeExceptionFuture<Try<RSP>>(Exception(ErrorCode::NoSuchService, service.ToString()));

    // deep copy because GetChannel is async
    std::shared_ptr<Message> reqCopy(req.New());
    reqCopy->CopyFrom(req);

    return _InnerCall<RSP>(stub, method, reqCopy, ep);
}

namespace {

// internal use
template <typename RSP>
Future<Try<RSP>> _InnerCall(ServiceStub* stub,
                            const StringView& method,
                            const std::shared_ptr<Message>& reqCopy,
                            const Endpoint& ep) {
    // select one channel and invoke method via it
    auto channelFuture = stub->GetChannel(ep);

    // The channelFuture need not to set timeout, because the TCP connect already set timeout
    return channelFuture.Then([method, reqCopy](Try<ClientChannel*>&& chan) {
        try {
            ClientChannel* channel = chan.Value();
            return channel->Invoke<RSP>(method, reqCopy);
        } catch(...) {
            return MakeExceptionFuture<Try<RSP>>(std::current_exception());
        }
    });
}

} // end namespace

} // end namespace rpc

} // end namespace ananas


#endif

