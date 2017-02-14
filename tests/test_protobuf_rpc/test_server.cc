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

class UpperServiceImpl : public ::ananas::rpc::test::UpperService
{
public:
    virtual void ToUpper(::google::protobuf::RpcController* ,
                         const ::ananas::rpc::test::EchoRequest* request,
                         ::ananas::rpc::test::EchoResponse* response,
                         ::google::protobuf::Closure* done) override final
    {
        std::string* answer = response->mutable_text();
        answer->resize(request->text().size());

        std::transform(request->text().begin(), request->text().end(), answer->begin(), ::toupper);

        DBG(logger) << "Service ToUpper result = " << *answer;

        done->Run();
    }

    virtual void AppendDots(::google::protobuf::RpcController* ,
                            const ::ananas::rpc::test::EchoRequest* request,
                            ::ananas::rpc::test::EchoResponse* response,
                            ::google::protobuf::Closure* done) override final
    {
        std::string* answer = response->mutable_text();
        *answer = request->text();
        answer->append("...................");

        DBG(logger) << "Service AppendDots result = " << *answer;

        done->Run();
    }
};

void OnCreateChannel(ananas::rpc::RpcChannel* chan)
{
    INF(logger) << "OnCreateChannel";
}

int main()
{
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_rpcserver_test");

    ananas::rpc::RpcService rpcServ;
    rpcServ.SetOnCreateChannel(OnCreateChannel);
    rpcServ.AddService(new UpperServiceImpl);

    ananas::EventLoop loop;
    if (loop.Listen("localhost", 8765,
                    std::bind(&ananas::rpc::RpcService::OnNewConnection, &rpcServ, std::placeholders::_1)))
        loop.Run();
    else
        ERR(logger) << "listen 8765 failed";

    return 0;
}

