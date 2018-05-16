#ifndef BERT_NAMESERVER_H
#define BERT_NAMESERVER_H

#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "protobuf_rpc/RpcServer.h"
#include "protobuf_rpc/RpcService.h"
#include "protobuf_rpc/ananas_rpc.pb.h"

#include "util/log/Logger.h"
#include "net/EventLoop.h"

std::shared_ptr<ananas::Logger> logger;

std::unordered_map<std::string, std::unordered_set<ananas::rpc::Endpoint> > g_map; 

class NameServiceImpl : public ::ananas::rpc::NameService
{
public:
  virtual void GetEndpoints(::google::protobuf::RpcController* controller,
                            const ::ananas::rpc::ServiceName* request,
                            ::ananas::rpc::EndpointList* response,
                            ::google::protobuf::Closure* done) override final
  {
      auto it = g_map.find(request->name());
      if (it != g_map.end())
      {
          for (const auto& e : it->second)
              response->add_endpoints()->CopyFrom(e);

          std::cout << "GetEndpoints " << response->DebugString() << std::endl;
      }

      done->Run();
  }

  virtual void Keepalive(::google::protobuf::RpcController* controller,
                         const ::ananas::rpc::HeartbeatList* request,
                         ::ananas::rpc::Status* response,
                         ::google::protobuf::Closure* done) override final
  {
      for (int i = 0; i < request->info_size(); ++ i)
      {
          const auto& heart = request->info(i);
          auto& endpoints = g_map[heart.servicename()];
          bool succ = endpoints.insert(heart.endpoint()).second;
          if (succ)
              std::cout << "Keepalive " << heart.servicename() << EndpointToString(heart.endpoint()) << std::endl;
      }

      done->Run();
  }
};

int main()
{
    // init log
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "annaas_nameserver");

    // init service
    auto srv = new ananas::rpc::Service(new NameServiceImpl);
    srv->SetEndpoint(ananas::rpc::EndpointFromString("tcp://127.0.0.1:9900"));

    // bootstrap server
    ananas::rpc::Server server;
    server.SetNumOfWorker(3);
    server.AddService(srv);
    server.Start();

    return 0;
}

#endif

