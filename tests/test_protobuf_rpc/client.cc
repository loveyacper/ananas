#include <ctype.h>
#include <string>
#include <algorithm>

#include "protobuf_rpc/ananas_rpc.pb.h"
#include "protobuf_rpc/RpcServer.h"
#include "protobuf_rpc/RpcService.h"
#include "protobuf_rpc/RpcServiceStub.h"
#include "test_rpc.pb.h"

#include "util/log/Logger.h"
#include "net/EventLoop.h"
#include "net/Application.h"
#include "net/Connection.h"

std::shared_ptr<ananas::Logger> logger;

const char* g_text = "hello ananas_rpc";

// for google::protobuf::Closure
void OnResponse(ananas::rpc::test::EchoResponse* response)
{
    USR(logger) << "OnResponse " << response->text();
}

void OnCreateChannel(ananas::rpc::ClientChannel* chan)
{
    DBG(logger) << "OnCreateChannel";
//    Future<RSP> ClientChannel::Invoke(const std::string& method, const ::google::protobuf::Message& request)
        
#if 0

    auto rpcServ = chan->Service();

    auto stub = rpcServ->GetServiceStub<ananas::rpc::test::TestService::Stub>("ananas.rpc.test.TestService");
    if (!stub)
    {
        ERR(logger) << "GetStub TestService failed";
    }
    else
    {
        ananas::rpc::test::EchoRequest* req = new ananas::rpc::test::EchoRequest();
        req->set_text(g_text);

        ananas::rpc::test::EchoResponse* rsp = new ananas::rpc::test::EchoResponse();

        stub->ToUpper(nullptr,
                      req,
                      rsp,
                      ::google::protobuf::NewCallback(OnResponse, rsp));

        req = new ananas::rpc::test::EchoRequest();
        req->set_text(g_text);

        rsp = new ananas::rpc::test::EchoResponse();
        stub->AppendDots(nullptr,
                         req,
                         rsp,
                         ::google::protobuf::NewCallback(OnResponse, rsp));
    }
#endif
}

void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(logger) << "OnConnFail to " << peer.GetPort();
    ananas::Application::Instance().Exit();
}
void OnResponse2(ananas::Try<ananas::rpc::test::EchoResponse>&& response)
{
    try {
        ananas::rpc::test::EchoResponse rsp = std::move(response);
        USR(logger) << "OnResponse " << rsp.text();
    } catch(const std::exception& exp) {
        USR(logger) << "OnResponse exception " << exp.what();
    }
}

int main(int ac, char* av[])
{
    if (ac > 1)
        g_text = av[1];

    // init log
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_rpcclient_test");

    // init service
    auto teststub = new ananas::rpc::ServiceStub(new ananas::rpc::test::TestService_Stub(nullptr));
    teststub->SetUrlList("tcp://127.0.0.1:8765");
    teststub->SetOnCreateChannel(OnCreateChannel);

    // init server 
    ananas::rpc::Server server;
    server.AddServiceStub(teststub);

    // request in future
    ananas::rpc::test::EchoRequest* req = new ananas::rpc::test::EchoRequest();
    req->set_text(g_text);
    auto future = ananas::rpc::Call<ananas::rpc::test::EchoResponse>("ananas.rpc.test.TestService", "ToUpper", *req);
    future.Then(OnResponse2);

    // event loop
    server.Start();

    return 0;
}

