#include <unistd.h>
#include <iostream>
#include <assert.h>

#include "net/DatagramSocket.h"
#include "net/EventLoop.h"
#include "net/log/Logger.h"

std::shared_ptr<ananas::Logger> logger;

void OnMessage(ananas::DatagramSocket* dg, const char* data, size_t len)
{
    INF(logger) << "server receive:[" << data << "]";
    // echo package
    dg->SendPacket(data, len);
}

void OnCreate(ananas::DatagramSocket* dg)
{
    INF(logger) << "A new udp " << dg->Identifier();
}

int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logFile, "log_server_udptest");

    const uint16_t port = 7001;
    ananas::SocketAddr serverAddr("127.0.0.1", port);

    ananas::EventLoop loop;
    if (!loop.ListenUDP(serverAddr, OnMessage, OnCreate))
    {
        ERR(logger) << "ListenUDP failed";
        return -1;
    }
    else
    {
        DBG(logger) << "ListenUDP succ";
    }

    loop.ScheduleNextTick([]() {
            INF(logger) << "Hello, I am listen on " << port;
            });

    loop.Run();

    return 0;
}

