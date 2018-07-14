#include <ctype.h>
#include <string>
#include <algorithm>

#include "protobuf_rpc/ananas_rpc.pb.h"
#include "protobuf_rpc/RpcServer.h"
#include "protobuf_rpc/RpcService.h"
#include "protobuf_rpc/RpcServiceStub.h"
#include "test_rpc.pb.h"

#include "util/Logger.h"
#include "util/TimeUtil.h"
#include "util/ThreadPool.h"
#include "util/ConfigParser.h"
#include "net/EventLoop.h"
#include "net/Application.h"
#include "net/Connection.h"

ananas::Time start, end;
int nowCount = 0;
const int totalCount = 200 * 10000;

std::shared_ptr<ananas::rpc::test::EchoRequest> req;

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
            USR(logger) << "Done OnResponse avg " << (totalCount * 0.1f / (end - start)) << " W/s";
            ananas::Application::Instance().Exit();
        }
        else if (nowCount < totalCount)
        {
            using namespace ananas;
            using namespace ananas::rpc;
            Call<test::EchoResponse>("ananas.rpc.test.TestService",
                                     "AppendDots",
                                     req)
                                    .Then(OnResponse);
        }

        if (nowCount % 50000 == 0)
        {
            end.Now();
            USR(logger) << "OnResponse avg " << (nowCount * 0.1f / (end - start)) << " W/s";
        }

    } catch(const std::exception& exp) {
        USR(logger) << "OnResponse exception " << exp.what();
    }
}

int main(int ac, char* av[])
{
    using namespace ananas;
    using namespace ananas::rpc;

    // init log
    LogManager::Instance().Start();
    logger = LogManager::Instance().CreateLog(logALL, logALL, "logger_rpcclient_test");

    // try init config
    ConfigParser config;
    if (ac > 1)
    {
        if (!config.Load(av[1]))
        {
            ERR(logger) << "Load config failed:" << av[1];
        }
    }

    // init service
    auto teststub = new ServiceStub(new test::TestService_Stub(nullptr));
    teststub->SetOnCreateChannel(OnCreateChannel);

    const int threads = 8;
    // init server 
    Server server;
    server.SetNumOfWorker(config.GetData<int>("threads", threads));
    server.AddServiceStub(teststub);
    server.SetNameServer("tcp://127.0.0.1:6379");

    // initiate request test
    req = std::make_shared<ananas::rpc::test::EchoRequest>();
    req->set_text(g_text);

    auto starter = [&]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        start.Now();
        for (int i = 0; i < threads; ++ i)
        {
            Call<test::EchoResponse>("ananas.rpc.test.TestService",
                                     "AppendDots",
                                     req)
                                    .Then(OnResponse);
        }
    };

    ThreadPool pool;
    pool.Execute(starter);

    // event loop
    server.Start(ac, av);
    pool.JoinAll();

    return 0;
}

