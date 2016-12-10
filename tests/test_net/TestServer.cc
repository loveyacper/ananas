#include <unistd.h>
#include <iostream>
#include <assert.h>

#include "net/ThreadPool.h"
#include "net/Connection.h"
#include "net/EventLoop.h"
#include "net/log/Logger.h"
            
ananas::Logger* log = nullptr;

ananas::PacketLen_t OnMessage(ananas::Connection* conn, const char* data, ananas::PacketLen_t len)
{
    // echo package
    conn->SendPacket(data, len);
    return len;
}

void OnNewConnection(ananas::Connection* conn)
{
    using ananas::Connection;

    std::string msg = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";

    conn->SetOnConnect([msg](Connection* conn) {
            INF(log) << "A new connection " << conn->Identifier();
            conn->SendPacket(msg.data(), msg.size());
            });

    conn->SetOnMessage(OnMessage);

    conn->SetOnDisconnect([](Connection* conn) {
            WRN(log) << "OnDisConnect " << conn->Identifier();
            });
}

void ThreadFunc()
{
    static uint16_t port = 6380;
    ananas::EventLoop loop;
    loop.Listen("localhost", port++, OnNewConnection);

    loop.ScheduleNextTick([]() {
            INF(log) << "Hello, I am listen on " << port - 1;
            });

#if 0
    // shutdown after 30s
    loop.ScheduleAfter(std::chrono::seconds(30), [&]() {
            INF(log) << "Now stop server.";
            loop.Stop();
            });
#endif

    loop.Run();
}

void SanityCheck(int& threads)
{
    if (threads < 1)
        threads = 1;
    else if (threads > 20)
        threads = 20;
}

int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    log = ananas::LogManager::Instance().CreateLog(logALL, logFile, "log_server_test");

    int threads = 1;
    if (ac > 1)
    {
        threads = std::stoi(av[1]);
        SanityCheck(threads);
    }

    for (int i = 0; i < threads; ++ i)
        ananas::ThreadPool::Instance().Execute(ThreadFunc);

    ananas::ThreadPool::Instance().JoinAll();
    return 0;
}

