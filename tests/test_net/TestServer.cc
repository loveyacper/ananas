#include <unistd.h>
#include <atomic>

#include "net/Connection.h"
#include "net/EventLoop.h"
#include "net/log/Logger.h"
            
std::shared_ptr<ananas::Logger> logger;

ananas::PacketLen_t OnMessage(ananas::Connection* conn, const char* data, ananas::PacketLen_t len)
{
    // echo package
    conn->SendPacket(data, len);
    return len;
}

void OnNewConnection(ananas::Connection* conn)
{
    using ananas::Connection;

    conn->SetOnMessage(OnMessage);
    conn->SetOnDisconnect([](Connection* conn) {
            WRN(logger) << "OnDisConnect " << conn->Identifier();
            });
}


int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_server_test");

    ananas::EventLoop loop;

    const uint16_t myport = 6380;
    if (loop.Listen("localhost", myport, OnNewConnection))
    {
        loop.ScheduleNextTick([myport]() {
                INF(logger) << "Hello, I am listen on " << myport;
                });
    }
    else
    {
        loop.ScheduleNextTick([&loop, myport]() {
                ERR(logger) << "Server stopped, can not listen on " << myport;
                ananas::EventLoop::ExitApplication();
                });
    }

    loop.Run();

    return 0;
}

