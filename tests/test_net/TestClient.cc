#include <unistd.h>
#include <iostream>
#include <assert.h>
#include <thread>
#include "net/ThreadPool.h"
#include "net/Connection.h"
#include "net/EventLoop.h"
#include "net/log/Logger.h"

std::shared_ptr<ananas::Logger> log;

ananas::PacketLen_t OnMessage(ananas::Connection* conn, const char* data, size_t len)
{
    DBG(log) << "Recv " << data;
    // echo package
    conn->SendPacket(data, len);
    return len;
}

void OnConnect(ananas::Connection* conn)
{
    INF(log) << "OnConnect " << conn->Identifier();
}

void OnDisConnect(ananas::Connection* conn)
{
    INF(log) << "OnDisConnect " << conn->Identifier();
}

void OnNewConnection(ananas::Connection* conn)
{
    conn->SetOnConnect(OnConnect);
    conn->SetOnMessage(OnMessage);
    conn->SetOnDisconnect(OnDisConnect);
}

    
void OnConnFail(ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    INF(log) << "OnConnFail " << peer.GetPort();

    // reconnect
    loop->ScheduleAfter(std::chrono::seconds(2), [=]() {
            loop->Connect(peer, OnNewConnection, OnConnFail);
            });
}

void ThreadFunc()
{
    const uint16_t port = 6380;

    auto& loop = ANANAS_EVENTLOOP;
    loop.Connect("localhost", port, OnNewConnection, OnConnFail);

    loop.Run();
}

int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    log = ananas::LogManager::Instance().CreateLog(logALL, logFile, "log_client_test");

    INF(log) << "Starting...";

    ananas::ThreadPool::Instance().Execute(ThreadFunc);

    return 0;
}

