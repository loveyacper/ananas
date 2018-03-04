#ifndef BERT_RPCSERVER_H
#define BERT_RPCSERVER_H

#include <google/protobuf/message.h>

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "RpcEndpoint.h"
#include "RpcServiceStub.h"
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

    // server-side
    bool AddService(ananas::rpc::Service* service);
    bool AddService(std::unique_ptr<Service>&& service);

    // client-side
    bool AddServiceStub(ananas::rpc::ServiceStub* service);
    bool AddServiceStub(std::unique_ptr<ServiceStub>&& service);
    ananas::rpc::ServiceStub* GetServiceStub(const std::string& name) const;

    // both
    void SetNumOfWorker(size_t n);
    void Start();
    void Shutdown();

private:
    ananas::Application& app_;
    std::unordered_map<std::string, std::unique_ptr<Service> > services_;
    std::unordered_map<std::string, std::unique_ptr<ServiceStub> > stubs_;

    // TODO name server
    std::vector<Endpoint> nameServers_;

    static Server* s_rpcServer;
};


#define RPCSERVER ::ananas::rpc::Server::Instance()



template <typename RSP>
Future<RSP> Call(const std::string& service,
                 const std::string& method,
                 const ::google::protobuf::Message& req)
{
    // 1. find service stub
    auto stub = RPCSERVER.GetServiceStub(service);
    if (!stub)
    {
        return MakeExceptionFuture<RSP>(std::runtime_error("No such service " + service));
    }
                
    // deep copy because get channel is async
    ::google::protobuf::Message* reqCopy = req.New();
    reqCopy->CopyFrom(req);

    // 2. select one channel and invoke method via it
    auto channelFuture = stub->GetChannel();
    return channelFuture.Then([method, reqCopy](ananas::Try<ClientChannel*>&& chan) {
                              try {
                                  std::unique_ptr<google::protobuf::Message> _(reqCopy);
                                  ClientChannel* channel = chan;
                                  return channel->Invoke<RSP>(method, *reqCopy); // TO be thread-safe
                              } catch(...) {
                                  return MakeExceptionFuture<RSP>(std::current_exception());
                              }
                          });
}

} // end namespace rpc

} // end namespace ananas


#endif

