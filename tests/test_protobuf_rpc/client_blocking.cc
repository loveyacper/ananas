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
#include "net/EventLoop.h"
#include "net/Application.h"
#include "net/Connection.h"

ananas::Time start, end;
const int totalCount = 10 * 10000;

std::shared_ptr<ananas::rpc::test::EchoRequest> req;
std::shared_ptr<ananas::Logger> logger;

const char* g_text = "hello ananas_rpc";

void OnCreateChannel(ananas::rpc::ClientChannel* chan) {
    DBG(logger) << "OnCreateRpcChannel " << (void*)chan;
}

void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer) {
    INF(logger) << "OnConnFail to " << peer.GetPort();
    ananas::Application::Instance().Exit();
}

int main(int ac, char* av[]) {
    using namespace ananas;
    using namespace ananas::rpc;
    using namespace ananas::rpc::test;

    // init log
    LogManager::Instance().Start();
    logger = LogManager::Instance().CreateLog(logALL, logALL, "logger_rpcclient_test");

    // init service
    auto teststub = new ServiceStub(new TestService_Stub(nullptr));
    teststub->SetOnCreateChannel(OnCreateChannel);

    // init server
    Server server;
    server.SetNumOfWorker(1);
    server.AddServiceStub(teststub);
    server.SetNameServer("tcp://127.0.0.1:6379");

    // initiate request test
    req = std::make_shared<EchoRequest>();
    req->set_text(g_text);

    auto starter = [&]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        start.Now();

        // DO NOT use blocking rpc call in event loop thread, will cause DEADLOCK!!!
        // See netty docs:
        // https://netty.io/4.0/api/io/netty/util/concurrent/BlockingOperationException.html
        assert (EventLoop::Self() == nullptr);

        for (int i = 1; i <= totalCount; ++ i) {
            try {
                EchoResponse rsp = Call<EchoResponse>("ananas.rpc.test.TestService",
                                                      "AppendDots",
                                                      req)
                                                     .Wait(std::chrono::seconds(3));
            } catch (const std::exception& e) {
                ERR(logger) << "fut.Wait exception " << e.what();
                break;
            }

            // for debug print
            if (i == totalCount) {
                end.Now();
                USR(logger) << "Done rpc call avg " << (totalCount * 0.1f / (end - start)) << " W/s";
                Application::Instance().Exit();
            } else if (i % 50000 == 0) {
                end.Now();
                USR(logger) << "rpc call avg " << (i * 0.1f / (end - start)) << " W/s";
            }
        }
    };

    ThreadPool pool;
    pool.Execute(starter);

    server.Start(ac, av);
    pool.JoinAll();

    return 0;
}

