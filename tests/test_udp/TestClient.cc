#include <unistd.h>
#include <iostream>
#include <assert.h>
#include <thread>
#include "net/DatagramSocket.h"
#include "net/EventLoopGroup.h"
#include "net/Application.h"
#include "net/log/Logger.h"

std::shared_ptr<ananas::Logger> logger;

ananas::SocketAddr  g_serverAddr;

void OnMessage(ananas::DatagramSocket* dg, const char* data, size_t len)
{
    INF(logger) << "client receive " << len;
    // echo package
    dg->SendPacket(data, len);
}

void OnCreate(ananas::DatagramSocket* dg)
{
    INF(logger) << "OnCreate " << dg->Identifier();
    dg->SendPacket("helloworld", 10, &g_serverAddr);
}

int main(int ac, char* av[])
{
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logFile, "log_client_udptest");

    INF(logger) << "Starting testudp ...";

    const uint16_t port = 7001;

    g_serverAddr.Init("127.0.0.1", port);

    ananas::EventLoopGroup group;
    group.CreateClientUDP(OnMessage, OnCreate);

    auto& app = ananas::Application::Instance();
    app.Run();

    return 0;
}

