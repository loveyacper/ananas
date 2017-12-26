#include <unistd.h>
#include <atomic>

#include "net/Connection.h"
#include "net/log/Logger.h"
#include "net/Application.h"
#include "net/EventLoopGroup.h"
            
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
    ananas::LogManager::Instance().Start();
    logger = ananas::LogManager::Instance().CreateLog(logALL, logALL, "logger_server_test");

    auto& app = ananas::Application::Instance();
    ananas::EventLoopGroup group(2);
    app.SetWorkerGroup(&group);

    app.Listen("localhost", 6380,
                OnNewConnection,
                [](bool succ, const ananas::SocketAddr& addr)
                {
                    WRN(logger) << (succ ? "Succ" : "Failed") << " listen on " << addr.ToString();
                    if (!succ)
                        ananas::Application::Instance().Exit();
                });

    app.Run();

    return 0;
}

