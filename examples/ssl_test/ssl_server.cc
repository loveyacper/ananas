#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "ssl/SSLContext.h"
#include "ssl/SSLManager.h"
#include "net/EventLoop.h"
#include "net/log/Logger.h"

int main()
{
    ananas::LogManager::Instance().Start();

    using ananas::ssl::SSLManager;
    SSLManager::Instance().GlobalInit();

    const char* ctx = "serverctx";
    if (!SSLManager::Instance().AddCtx(ctx, SSLv3_method(), "ca.pem", "server-cert.pem", "server-key.pem"))
    {
        std::cerr << "Load certs failed\n";
        return -1;
    }

    ananas::EventLoop loop;

    if (!loop.Listen("loopback", 443,  std::bind(&ananas::ssl::OnNewSSLConnection,
                                                 ctx,
                                                 SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                                                 true,
                                                 std::placeholders::_1)))
    {
        std::cerr << "Listen 443 failed\n";
        ananas::EventLoop::ExitApplication();
    }

    loop.Run();
    return 0;
}

