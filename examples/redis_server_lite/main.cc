#include <iostream>
#include "RedisContext.h"
#include "net/EventLoop.h"

#include "RedisLog.h"

std::shared_ptr<ananas::Logger> logger;

void OnConnect(std::shared_ptr<RedisContext> ctx, ananas::Connection* conn)
{
    std::cout << "RedisContext.OnConnect " << conn->Identifier() << std::endl;
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

int main()
{
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_server_test");

    ananas::EventLoop loop;

    const uint16_t port = 6379;
    if (!loop.Listen("loopback", port, OnNewConnection))
    {
        std::cerr << "Server stopped, can not listen on " << port;
        ananas::EventLoop::ExitApplication();
    }
        
    INF(logger) << "listen on " << port;
       
    loop.Run();
    return 0;
}

