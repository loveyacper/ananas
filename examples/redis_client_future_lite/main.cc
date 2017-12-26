#include <iostream>
#include "RedisContext.h"
#include "net/EventLoop.h"
#include "net/Application.h"


void GetAndSetName(const std::shared_ptr<RedisContext>& ctx)
{
    // set name first, then get name.
    ctx->Set("name", "bertyoung").Then(
            [ctx](const std::pair<ResponseType, std::string>& rsp) {
                RedisContext::PrintResponse(rsp);
                return ctx->Get("name"); // get name, return another future
            }).Then(
                RedisContext::PrintResponse
                );
}

void WaitMultiRequests(const std::shared_ptr<RedisContext>& ctx)
{
    // issue 2 requests, when they all return, callback
    auto fut1 = ctx->Set("city", "shenzhen");
    auto fut2 = ctx->Get("city");

    ananas::WhenAll(fut1, fut2).Then(
                    [](const std::tuple<ananas::Try<std::pair<ResponseType, std::string> >,
                                  ananas::Try<std::pair<ResponseType, std::string> > >& results) {
                        std::cout << "All requests returned:\n";
                        RedisContext::PrintResponse(std::get<0>(results));
                        RedisContext::PrintResponse(std::get<1>(results));
            });
}

void OnConnect(std::shared_ptr<RedisContext> ctx, ananas::Connection* conn)
{
    std::cout << "RedisContext.OnConnect " << conn->Identifier() << std::endl;

    // set item = diamond and callback PrintResponse
    ctx->Set("item", "diamond").Then(
            RedisContext::PrintResponse
            ).OnTimeout(std::chrono::seconds(3), []() {
                std::cout << "OnTimeout request, now exit test\n";
                ananas::Application::Instance().Exit();
                },
                conn->GetLoop());

    GetAndSetName(ctx);

    WaitMultiRequests(ctx);
}


void OnNewConnection(ananas::Connection* conn)
{
    std::cout << "OnNewConnection " << conn->Identifier() << std::endl;

    auto ctx = std::make_shared<RedisContext>(conn);

    conn->SetOnConnect(std::bind(&OnConnect, ctx, std::placeholders::_1));
    conn->SetOnMessage(std::bind(&RedisContext::OnRecv, ctx, std::placeholders::_1,
                                                             std::placeholders::_2,
                                                             std::placeholders::_3));
}

    
void OnConnFail(int maxTryCount, ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    std::cout << "OnConnFail from " << peer.GetPort() << std::endl;
    if (-- maxTryCount <= 0)
    {
        std::cerr << "ReConnect failed, exit app\n";
        ananas::Application::Instance().Exit();
    }

    // reconnect
    loop->ScheduleAfter(std::chrono::seconds(2), [=]() {
        loop->Connect(peer, OnNewConnection, std::bind(&OnConnFail,
                                                        maxTryCount,
                                                        std::placeholders::_1,
                                                        std::placeholders::_2));
    });
}

int main()
{
    int maxTryCount = 5;

    auto& app = ananas::Application::Instance();
    app.Connect("loopback", 6379, OnNewConnection, std::bind(&OnConnFail,
                                                              maxTryCount,
                                                              std::placeholders::_1,
                                                              std::placeholders::_2));
    app.Run();

    return 0;
}

