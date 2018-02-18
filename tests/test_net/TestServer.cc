#include <unistd.h>
#include <atomic>

#include "net/Connection.h"
#include "net/Application.h"
#include "util/log/Logger.h"
            
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
    conn->SetOnDisconnect([](Connection* conn)
                          {
                              WRN(logger) << "OnDisConnect " << conn->Identifier();
                          });
}


int main(int ac, char* av[])
{
    size_t workers = 1;
    if (ac > 1)
        workers = (size_t)std::stoi(av[1]);

    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_server_test");

    auto& app = ananas::Application::Instance();
    app.SetNumOfWorker(workers);
    app.Listen("localhost", 6380, OnNewConnection);

    app.Run();

    return 0;
}

