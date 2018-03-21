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

void OnCreateChannel(ananas::rpc::ClientChannel* chan)
{
    DBG(logger) << "OnCreateRpcChannel " << (void*)chan;
}

void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(logger) << "OnConnFail to " << peer.GetPort();
    ananas::Application::Instance().Exit();
}

void OnResponse(ananas::Try<ananas::rpc::test::EchoResponse>&& response)
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

    using namespace ananas;

    // init log
    LogManager::Instance().Start();
    logger = LogManager::Instance().CreateLog(logALL, logALL, "logger_rpcclient_test");

    // init service
    auto teststub = new rpc::ServiceStub(new rpc::test::TestService_Stub(nullptr));
    teststub->SetUrlList("tcp://127.0.0.1:8765");
    teststub->SetOnCreateChannel(OnCreateChannel);

    // init server 

    rpc::Server server;
    server.AddServiceStub(teststub);

    // initiate request test
    rpc::test::EchoRequest req;
    req.set_text(g_text);

    rpc::Call<rpc::test::EchoResponse>("ananas.rpc.test.TestService",   // service name
                                       "ToUpper",                       // method name
                                       req)                            // request args
                                       .Then(OnResponse);               // response handler

    rpc::Call<rpc::test::EchoResponse>("ananas.rpc.test.TestService",   // service name
                                       "AppendDots",                    // method name
                                       req)                            // request args
                                       .Then(OnResponse);               // response handler

    // event loop
    server.Start();

    return 0;
}

