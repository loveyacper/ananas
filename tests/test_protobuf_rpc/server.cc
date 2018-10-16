#include <ctype.h>
#include <string>
#include <algorithm>

#include "protobuf_rpc/RpcServer.h"
#include "protobuf_rpc/RpcService.h"
#include "test_rpc.pb.h"

#include "util/Logger.h"
#include "net/EventLoop.h"

std::shared_ptr<ananas::Logger> logger;

class TestServiceImpl : public ::ananas::rpc::test::TestService {
public:
    virtual void ToUpper(::google::protobuf::RpcController*,
                         const ::ananas::rpc::test::EchoRequest* request,
                         ::ananas::rpc::test::EchoResponse* response,
                         ::google::protobuf::Closure* done) override final {
        std::string* answer = response->mutable_text();
        answer->resize(request->text().size());

#if 1
        // simulate async process
        // MUST copy request, because the fucking raw pointer.
        // No need copy response, because `done` manage it.
        auto loop = ananas::EventLoop::GetCurrentEventLoop();
        loop->ScheduleAfterWithRepeat<1>(std::chrono::seconds(2),
        [request = *request, answer, done]() {
            std::transform(request.text().begin(), request.text().end(), answer->begin(), ::toupper);
            done->Run();
        });
#else
        std::transform(request->text().begin(), request->text().end(), answer->begin(), ::toupper);

        DBG(logger) << "Service ToUpper result = " << *answer;

        done->Run();
#endif
    }

    virtual void AppendDots(::google::protobuf::RpcController*,
                            const ::ananas::rpc::test::EchoRequest* request,
                            ::ananas::rpc::test::EchoResponse* response,
                            ::google::protobuf::Closure* done) override final {
        std::string* answer = response->mutable_text();
        *answer = request->text();
        answer->append("...................");

        //DBG(logger) << "Service AppendDots result = " << *answer;
        done->Run();
    }
};

int main(int ac, char* av[]) {
    int threads = 6;
    if (ac > 1)
        threads = std::stoi(av[1]);
    // init log
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_rpcserver_test");

    // init service
    auto testsrv = new ananas::rpc::Service(new TestServiceImpl);
    testsrv->SetEndpoint(ananas::rpc::EndpointFromString("tcp://127.0.0.1:9987"));

    // bootstrap server
    ananas::rpc::Server server;
    server.SetNumOfWorker(threads);
    server.AddService(testsrv);

    server.SetNameServer("tcp://127.0.0.1:6379");
    server.SetHealthService("tcp://127.0.0.1:8000");
    server.Start(ac, av);

    return 0;
}

