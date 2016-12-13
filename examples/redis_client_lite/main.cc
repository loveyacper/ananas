#include <iostream>
#include "RedisContext.h"


void OnConnect(std::shared_ptr<RedisContext> ctx, ananas::Connection* conn)
{
    std::cout << "RedisContext.OnConnect " << conn->Identifier() << std::endl;
    
    // issue set request
    ctx->Set("name", "bertyoung").Then(RedisContext::PrintResponse);

    // issue get request
    ctx->Get("name").Then(RedisContext::PrintResponse);

    // issue 2 requests, and wait all
    auto fut1 = ctx->Set("city", "shenzhen");
    auto fut2 = ctx->Get("city");

    ananas::WhenAll(fut1, fut2).Then(
                    [](std::tuple<ananas::Try<std::pair<ResponseType, std::string> >,
                                  ananas::Try<std::pair<ResponseType, std::string> > >& results) {
                        std::cout << "All requests returned\n";
                        RedisContext::PrintResponse(std::get<0>(results));
                        RedisContext::PrintResponse(std::get<1>(results));
            });
}


void OnNewConnection(ananas::Connection* conn)
{
    std::cout << "OnNewConnection " << conn->Identifier() << std::endl;

    std::shared_ptr<RedisContext> ctx = std::make_shared<RedisContext>(conn);

    conn->SetOnConnect(std::bind(&OnConnect, ctx, std::placeholders::_1));
    conn->SetOnMessage(std::bind(&RedisContext::OnRecv, ctx, std::placeholders::_1,
                                                             std::placeholders::_2,
                                                             std::placeholders::_3));
}

    
void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    std::cout << "OnConnFail from " << peer.GetPort() << std::endl;

    // reconnect
    loop->ScheduleAfter(std::chrono::seconds(2), [=]() {
        loop->Connect(peer, OnNewConnection, OnConnFail);
    });
}

int main()
{
    ananas::EventLoop loop;
    loop.Connect("loopback", 6379, OnNewConnection, OnConnFail);

    loop.Run();

    return 0;
}

