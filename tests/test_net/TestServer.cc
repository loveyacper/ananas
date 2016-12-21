#include <unistd.h>
#include <atomic>

#include "net/ThreadPool.h"
#include "net/Connection.h"
#include "net/EventLoop.h"
#include "net/log/Logger.h"
//#include "util/ThreadLocalSingleton.h"
            
std::shared_ptr<ananas::Logger> log;

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
    static std::atomic<uint16_t> port{ 6380 };

    auto& loop = ANANAS_EVENTLOOP;

    const uint16_t myport = port ++;
    if (loop.Listen("localhost", myport, OnNewConnection))
    {
        loop.ScheduleNextTick([myport]() {
                INF(log) << "Hello, I am listen on " << myport;
                });
    }

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
    log = ananas::LogManager::Instance().CreateLog(logALL, logALL, "log_server_test");

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

