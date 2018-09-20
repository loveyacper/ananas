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

namespace ananas {

class Application;

namespace rpc {

using google::protobuf::Message;

class Service;
class ServiceStub;

class Server {
public:
    Server();
    static Server& Instance();

    // base loop
    EventLoop* BaseLoop();
    // next loop
    EventLoop* Next();

    void SetOnInit(std::function<bool (int, char*[])> );
    void SetOnExit(std::function<void ()> );

    // nameserver
    void SetNameServer(const std::string& url);
    void SetOnCreateNameServerChannel(std::function<void (ClientChannel*)> );

    // server-side
    bool AddService(Service* service);
    bool AddService(std::unique_ptr<Service>&& service);

    // client-side
    bool AddServiceStub(ServiceStub* service);
    bool AddServiceStub(std::unique_ptr<ServiceStub>&& service);
    // We don't add stubs during runtime, so need not to be thread-safe
    ServiceStub* GetServiceStub(const StringView& name) const;

    // both
    void SetNumOfWorker(size_t n);
    void Start(int argc, char* argv[]);
    void Shutdown();

private:
    Application& app_;
    std::unordered_map<StringView, std::unique_ptr<Service> > services_;
    std::unordered_map<StringView, std::unique_ptr<ServiceStub> > stubs_;

    ServiceStub* nameServiceStub_ {nullptr};
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


// The entry point for initiate a rpc call.
// `service` is the full name of a service, eg. "test.videoservice"
// `method` is one of the `service`'s method's name, eg. "changeVideoQuality"
// `RSP` is the type of response. why use `Try` type? Because may throw exception value

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

