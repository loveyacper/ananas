#include <iostream>
#include "RedisContext.h"
#include "net/EventLoop.h"
#include "net/Application.h"

void GetSomeRedisKey(std::shared_ptr<RedisContext> ctx, const std::string& key)
{
    std::cout << "Coroutine is primed\n";

    auto rsp = ananas::Coroutine::Yield(ctx->Get(key));

    std::cout << "Coroutine is resumed\n";

    if (rsp)
    {
        std::cout << "Value for key " << key
                  << " is " << *std::static_pointer_cast<std::string>(rsp) << std::endl;
    }
    else
    {
        std::cerr << "got rsp failed\n";
    }
}

void OnConnect(std::shared_ptr<RedisContext> ctx, ananas::Connection* conn)
{
    std::cout << "RedisContext.OnConnect " << conn->Identifier() << std::endl;

    if (ctx->StartCoroutine(GetSomeRedisKey, ctx, "name"))
        std::cout << "StartCoroutine success\n";
    else
        std::cerr << "StartCoroutine failed\n";
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

int main(int ac, char* av[])
{
    int maxTryCount = 5;

    auto& app = ananas::Application::Instance();
    app.Connect("loopback", 6379, OnNewConnection, std::bind(&OnConnFail,
                                                              maxTryCount,
                                                              std::placeholders::_1,
                                                              std::placeholders::_2));

    app.Run(ac, av);

    return 0;
}

