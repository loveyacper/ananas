#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ssl/SSLContext.h"
#include "ssl/SSLManager.h"
#include "net/EventLoop.h"
#include "net/Connection.h"
#include "net/log/Logger.h"

const uint16_t kPort = 8443;

void NewSSLConnection(const std::string& ctxName, int verifyMode, bool incoming, ananas::Connection* c)
{
    ananas::ssl::OnNewSSLConnection(ctxName, verifyMode, incoming, c);

    auto open = c->GetUserData<ananas::ssl::OpenSSLContext>();
    open->SetLogicProcess([](ananas::Connection* c, const char* data, size_t len) {
            std::cout << "Process len " << len << std::endl;
            std::cout << "Process data " << data << std::endl;
            return len;
    });
}


void OnConnFail(int maxTryCount, ananas::EventLoop* loop, const ananas::SocketAddr& peer)
{
    if (-- maxTryCount <= 0)
    {
        std::cerr << "ReConnect failed, exit app\n";
        ananas::EventLoop::ExitApplication();
    }

    // reconnect
    loop->ScheduleAfter(std::chrono::seconds(2), [=]() {
        loop->Connect("loopback", kPort, std::bind(&ananas::ssl::OnNewSSLConnection,
                                                  "clientctx",
                                                  SSL_VERIFY_PEER,
                                                  false,
                                                  std::placeholders::_1),
                                         std::bind(&OnConnFail,
                                                  maxTryCount,
                                                  std::placeholders::_1,
                                                  std::placeholders::_2));
    });
}

int main()
{
    ananas::LogManager::Instance().Start();

    using ananas::ssl::SSLManager;
    SSLManager::Instance().GlobalInit();

    const char* ctx = "clientctx";
    if (!SSLManager::Instance().AddCtx(ctx, "ca.pem", "client-cert.pem", "client-key.pem"))
    {
        std::cerr << "Load certs failed\n";
        return -1;
    }

    ananas::EventLoop loop;

    int maxTryCount = 0;
    loop.Connect("loopback", kPort, std::bind(NewSSLConnection,
                                              ctx,
                                              SSL_VERIFY_PEER,
                                              false,
                                              std::placeholders::_1),
                                    std::bind(&OnConnFail,
                                              maxTryCount,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
    loop.Run();

    return 0;
}

