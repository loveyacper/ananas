#ifndef BERT_RPCSERVER_H
#define BERT_RPCSERVER_H

#include <google/protobuf/message.h>

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "RpcEndpoint.h"
#include "RpcServiceStub.h"
#include "util/StringView.h"
#include "future/Future.h"

namespace ananas
{

class Application;

namespace rpc
{

class Service;
class ServiceStub;

class Server
{
public:
    Server();
    static Server& Instance();

    // base loop
    EventLoop* BaseLoop();

    // nameserver
    void SetNameServer(const std::string& url);
    void SetOnCreateNameServerChannel(std::function<void (ClientChannel*)> );

    // server-side
    bool AddService(ananas::rpc::Service* service);
    bool AddService(std::unique_ptr<Service>&& service);

    // client-side
    bool AddServiceStub(ananas::rpc::ServiceStub* service);
    bool AddServiceStub(std::unique_ptr<ServiceStub>&& service);
    // We don't add stubs during runtime, so need not to be thread-safe
    //ananas::rpc::ServiceStub* GetServiceStub(const std::string& name) const;
    ananas::rpc::ServiceStub* GetServiceStub(const ananas::StringView& name) const;

    // both
    void SetNumOfWorker(size_t n);
    void Start();
    void Shutdown();

private:
    ananas::Application& app_;
    std::unordered_map<ananas::StringView, std::unique_ptr<Service> > services_;
    std::unordered_map<ananas::StringView, std::unique_ptr<ServiceStub> > stubs_;

    ServiceStub* nameServiceStub_ {nullptr};
    std::function<void (ClientChannel*)> onCreateNameServiceChannel_;

    // The services's info
    std::vector<KeepaliveInfo> keepaliveInfo_;

    static Server* s_rpcServer;
};


#define RPC_SERVER ::ananas::rpc::Server::Instance()

// internal use
template <typename RSP>
Future<ananas::Try<RSP>> _InnerCall(ananas::rpc::ServiceStub* stub,
                                    const ananas::StringView& method,
                                    const std::shared_ptr<::google::protobuf::Message>& reqCopy,
                                    const Endpoint& ep = Endpoint::default_instance());


// The entry point for initiate a rpc call.
// `service` is the full name of a service, eg. "test.videoservice"
// `method` is one of the `service`'s method's name, eg. "changeVideoQuality"
// `RSP` is the type of response. why use `Try` type? Because may throw exception value

template <typename RSP>
Future<ananas::Try<RSP>> Call(const ananas::StringView& service,
                              const ananas::StringView& method,
                              const std::shared_ptr<::google::protobuf::Message>& reqCopy,
                              const Endpoint& ep = Endpoint::default_instance())
{
    // 1. find service stub
    auto stub = RPC_SERVER.GetServiceStub(service);
    if (!stub)
        return MakeExceptionFuture<ananas::Try<RSP>>(std::runtime_error("No such service [" + service.ToString() + "]"));

    return _InnerCall<RSP>(stub, method, reqCopy, ep);
}

template <typename RSP>
Future<ananas::Try<RSP>> Call(const ananas::StringView& service,
                              const ananas::StringView& method,
                              const ::google::protobuf::Message& req,
                              const Endpoint& ep = Endpoint::default_instance())
{
    // 1. find service stub
    auto stub = RPC_SERVER.GetServiceStub(service);
    if (!stub)
        return MakeExceptionFuture<ananas::Try<RSP>>(std::runtime_error("No such service [" + service.ToString() + "]"));

    // deep copy because GetChannel is async
    std::shared_ptr<::google::protobuf::Message> reqCopy(req.New());
    reqCopy->CopyFrom(req);

    return _InnerCall<RSP>(stub, method, reqCopy, ep);
}


// internal use
template <typename RSP>
Future<ananas::Try<RSP>> _InnerCall(ananas::rpc::ServiceStub* stub,
                                    const ananas::StringView& method,
                                    const std::shared_ptr<::google::protobuf::Message>& reqCopy,
                                    const Endpoint& ep)
{
    // select one channel and invoke method via it
    auto channelFuture = stub->GetChannel(ep);

    // The channelFuture need not to set timeout, because the TCP connect already set timeout
    return channelFuture.Then([method, reqCopy](ananas::Try<ClientChannel*>&& chan) {
                              try {
                                  ClientChannel* channel = chan.Value();
                                  return channel->Invoke<RSP>(method, reqCopy);
                              } catch(...) {
                                  return MakeExceptionFuture<ananas::Try<RSP>>(std::current_exception());
                              }
                          });
}


} // end namespace rpc

} // end namespace ananas


#endif

