#include <unistd.h>
#include <iostream>
#include <assert.h>
#include <thread>

#include "net/Connection.h"
#include "net/EventLoop.h"
#include "net/Application.h"
#include "net/log/Logger.h"

std::shared_ptr<ananas::Logger> logger;

ananas::PacketLen_t OnMessage(ananas::Connection* conn, const char* data, size_t len)
{
    // echo package
    conn->SendPacket(data, len);
    return len;
}

void OnWriteComplete(ananas::Connection* conn)
{
    //INF(logger) << "OnWriteComplete for " << conn->Identifier();
}

void OnConnect(ananas::Connection* conn)
{
    INF(logger) << "OnConnect " << conn->Identifier();
            
    std::string tmp = "abcdefghijklmnopqrstuvwxyz";
    std::string msg = tmp + tmp + tmp + tmp;
    conn->SendPacket(msg.data(), msg.size());
}

void OnDisConnect(ananas::Connection* conn)
{
    INF(logger) << "OnDisConnect " << conn->Identifier();
}

void OnNewConnection(ananas::Connection* conn)
{
    conn->SetOnConnect(OnConnect);
    conn->SetOnMessage(OnMessage);
    conn->SetOnDisconnect(OnDisConnect);
    //conn->SetOnWriteComplete(OnWriteComplete);
}

    
void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(logger) << "OnConnFail " << peer.GetPort();

    // reconnect
    loop->ScheduleAfter(std::chrono::seconds(2),
                        [=]()
                        {
                            loop->Connect(peer,
                                          OnNewConnection,
                                          OnConnFail,
                                          ananas::DurationMs(3000));
                        });
}

void SanityCheck(int& threads)
{
    if (threads < 1)
        threads = 1;
    else if (threads > 100)
        threads = 100;
}

int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_client_test");

    int threads = 1;
    if (ac > 1)
    {
        threads = std::stoi(av[1]);
        SanityCheck(threads);
    }

    const uint16_t port = 6380;
    const int kConns = 10;

    auto& app = ananas::Application::Instance();
    app.SetNumOfWorker(threads);
    for (int i = 0; i < kConns; ++ i)
        app.Connect("localhost", port, OnNewConnection, OnConnFail, ananas::DurationMs(3000));

    app.Run();

    return 0;
}

