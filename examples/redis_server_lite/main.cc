#include <iostream>
#include <unistd.h>
#include "RedisContext.h"
#include "net/EventLoop.h"
#include "net/Application.h"

#include "RedisLog.h"

std::shared_ptr<ananas::Logger> logger;

void OnConnect(std::shared_ptr<RedisContext> ctx, ananas::Connection* conn)
{
    std::cout << "RedisContext.OnConnect " << conn->Identifier() << std::endl;
}

void OnNewConnection(ananas::Connection* conn)
{
    std::cout << "OnNewConnection " << conn->Identifier() << std::endl;

    auto ctx = std::make_shared<RedisContext>(conn);

    conn->SetOnConnect(std::bind(&OnConnect, ctx, std::placeholders::_1));
    conn->SetOnMessage(std::bind(&RedisContext::OnRecvAll, ctx, std::placeholders::_1,
                                                             std::placeholders::_2,
                                                             std::placeholders::_3));
}

bool Init(int ac, char* av[])
{
    uint16_t port = 6379;

    int ch = 0;
    while ((ch = getopt(ac, av, "p:")) != -1)
    {
        switch (ch)
        {
            case 'p':
                port = std::stoi(optarg);
                break;

            default:
                return false;
        }
    }

    auto& app = ananas::Application::Instance();
    app.Listen("loopback", port, OnNewConnection);
    return true;
}

int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_server_test");

    auto& app = ananas::Application::Instance();
    app.SetOnInit(Init);

    app.Run(ac, av);

    return 0;
}

