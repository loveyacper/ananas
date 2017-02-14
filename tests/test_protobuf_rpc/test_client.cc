#include <ctype.h>
#include <string>
#include <algorithm>

#include "protobuf_rpc/AnanasRpc.h"
#include "protobuf_rpc/ananas_rpc.pb.h"
#include "test_rpc.pb.h"

#include "net/log/Logger.h"
#include "net/EventLoop.h"
#include "net/Connection.h"

std::shared_ptr<ananas::Logger> logger;

const char* g_text = "hello ananas_rpc";

class UpperServiceImpl : public ananas::rpc::test::UpperService
{
};

// for google::protobuf::Closure
void OnResponse(ananas::rpc::test::EchoResponse* response)
{
    USR(logger) << "OnResponse " << response->text();
}

void OnCreateChannel(ananas::rpc::RpcChannel* chan)
{
    DBG(logger) << "OnCreateChannel";

    auto rpcServ = chan->Service();

    auto stub = rpcServ->GetServiceStub<ananas::rpc::test::UpperService>("ananas.rpc.test.UpperService");
    if (!stub)
    {
        ERR(logger) << "GetStub failed";
    }
    else
    {
        {
            ananas::rpc::test::EchoRequest* req = new ananas::rpc::test::EchoRequest();
            req->set_text(g_text);

            ananas::rpc::test::EchoResponse* rsp = new ananas::rpc::test::EchoResponse();

            stub->ToUpper(nullptr,
                          req,
                          rsp,
                          ::google::protobuf::NewCallback(OnResponse, rsp));
        }

        {
            ananas::rpc::test::EchoRequest* req = new ananas::rpc::test::EchoRequest();
            req->set_text(g_text);

            ananas::rpc::test::EchoResponse* rsp = new ananas::rpc::test::EchoResponse();

            stub->AppendDots(nullptr,
                          req,
                          rsp,
                          ::google::protobuf::NewCallback(OnResponse, rsp));
        }
    }
}

void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(logger) << "OnConnFail to " << peer.GetPort();
    ananas::EventLoop::ExitApplication();
}

int main(int ac, char* av[])
{
    if (ac > 1)
        g_text = av[1];

    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_rpcclient_test");

    ananas::rpc::RpcService rpcServ;
    rpcServ.SetOnCreateChannel(OnCreateChannel);
    rpcServ.AddService(new UpperServiceImpl());

    ananas::EventLoop loop;
    loop.Connect("localhost", 8765,
            std::bind(&ananas::rpc::RpcService::OnNewConnection, &rpcServ, std::placeholders::_1),
            OnConnFail);

    loop.Run();

    return 0;
}

