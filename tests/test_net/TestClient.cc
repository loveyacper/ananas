#include <unistd.h>
#include <iostream>
#include <assert.h>
#include <thread>
#include "net/ThreadPool.h"
#include "net/Connection.h"
#include "net/EventLoop.h"
#include "net/log/Logger.h"

std::shared_ptr<ananas::Logger> logger;

ananas::PacketLen_t OnMessage(ananas::Connection* conn, const char* data, size_t len)
{
    DBG(logger) << "Recv " << data;
    // echo package
    conn->SendPacket(data, len);
    return len;
}

void OnConnect(ananas::Connection* conn)
{
    INF(logger) << "OnConnect " << conn->Identifier();
            
    std::string msg = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
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
}

    
void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(logger) << "OnConnFail " << peer.GetPort();

    // reconnect
    loop->ScheduleAfter(std::chrono::seconds(2), [=]() {
            loop->Connect(peer, OnNewConnection, OnConnFail);
            });
}

void ThreadFunc()
{
    const uint16_t port = 6380;
    const int kConnsPerThread = 10;

    ananas::EventLoop loop;
    for (int i = 0; i < kConnsPerThread; ++ i)
        loop.Connect("localhost", port, OnNewConnection, OnConnFail);

    loop.Run();
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
    daemon(1, 0);

    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logFile, "logger_client_test");

    int threads = 1;
    if (ac > 1)
    {
        threads = std::stoi(av[1]);
        SanityCheck(threads);
    }

    for (int i = 0; i < threads; ++ i)
        ananas::ThreadPool::Instance().Execute(ThreadFunc);

    return 0;
}

