#include <unistd.h>
#include <iostream>
#include <assert.h>
#include <thread>
#include "net/ThreadPool.h"
#include "net/Connection.h"
#include "net/EventLoop.h"
#include "net/log/Logger.h"

ananas::Logger* log = nullptr;

ananas::PacketLen_t OnMessage(ananas::Connection* conn, const char* data, size_t len)
{
    //INF(log) << "receive " << data;
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
    const uint16_t port = 6379;

    ananas::EventLoop loop;
    loop.Connect("127.0.0.1", port, OnNewConnection, OnConnFail);

    loop.Run();
}

int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    log = ananas::LogManager::Instance().CreateLog(logALL, logFile, "client_test_log");

    ananas::ThreadPool::Instance().Execute(ThreadFunc);
    ananas::ThreadPool::Instance().JoinAll();

    return 0;
}

