#include <ctype.h>
#include <string>
#include <algorithm>

#include "protobuf_rpc/ananas_rpc.pb.h"
#include "protobuf_rpc/RpcServer.h"
#include "protobuf_rpc/RpcService.h"
#include "protobuf_rpc/RpcServiceStub.h"
#include "test_rpc.pb.h"

#include "util/log/Logger.h"
#include "util/TimeUtil.h"
#include "net/EventLoop.h"
#include "net/Application.h"
#include "net/Connection.h"

ananas::Time start, end;
int nowCount = 0;
const int totalCount = 20 * 10000;

ananas::rpc::test::EchoRequest req;

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
        ++ nowCount;
        if (nowCount == totalCount)
        {
            end.Now();
            USR(logger) << "OnResponse avg " << (totalCount * 0.1f / (end - start)) << " W/s";
            ananas::Application::Instance().Exit();
        }
        else
        {
            using namespace ananas;
            using namespace ananas::rpc;
            Call<test::EchoResponse>("ananas.rpc.test.TestService",
                                     "AppendDots",
                                     req)
                                    .Then(OnResponse);
        }
            
       // USR(logger) << "OnResponse " << rsp.text();
    } catch(const std::exception& exp) {
        USR(logger) << "OnResponse exception " << exp.what();
        //std::abort();
    }
}

int main(int ac, char* av[])
{
    if (ac > 1)
        g_text = av[1];

    using namespace ananas;
    using namespace ananas::rpc;

    // init log
    LogManager::Instance().Start();
    logger = LogManager::Instance().CreateLog(logALL, logALL, "logger_rpcclient_test");

    // init service
    auto teststub = new ServiceStub(new test::TestService_Stub(nullptr));
    //teststub->SetUrlList("tcp://127.0.0.1:8765");
    //teststub->SetOnCreateChannel(OnCreateChannel);

    // init server 

    Server server;
    server.AddServiceStub(teststub);
    server.SetNameServer("tcp://127.0.0.1:9900");

    // initiate request test
    req.set_text(g_text);

    start.Now();
#if 0
    Call<test::EchoResponse>("ananas.rpc.test.TestService",
                             "Echo",
                             req)
                            .Then(OnResponse);
#endif
    Call<test::EchoResponse>("ananas.rpc.test.TestService",
                             "AppendDots",
                             req)
                            .Then(OnResponse);
#if 0
    Call<test::EchoResponse>("ananas.rpc.test.TestService",  // service name
                             "ToUpperFuck",                      // method name
                             req)                            // request args
                            .Then(OnResponse);               // response handler
#endif


    // event loop
    server.Start();

    return 0;
}

