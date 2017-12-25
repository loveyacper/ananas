
#ifndef BERT_SOCKET_H
#define BERT_SOCKET_H

#include <arpa/inet.h>
#include <sys/resource.h>
#include <string.h>
#include <string>
#include <memory>

namespace ananas
{

std::string ConvertIp(const char* ip);

struct SocketAddr
{
    static const uint16_t kInvalidPort = -1;

    SocketAddr()
    {
        memset(&addr_, 0, sizeof addr_);
    }
    
    SocketAddr(const sockaddr_in& addr)
    {
        Init(addr);
    }

    SocketAddr(uint32_t netip, uint16_t netport)
    {
        Init(netip, netport);
    }

    SocketAddr(const char* ip, uint16_t hostport)
    {
        Init(ip, hostport);
    }

    void Init(const sockaddr_in& addr)
    {
        memcpy(&addr_, &addr, sizeof(addr));
    }

    void Init(uint32_t netip, uint16_t netport)
    {
        addr_.sin_family = AF_INET;       
        addr_.sin_addr.s_addr = netip;       
        addr_.sin_port   = netport;
    }

    void Init(const char* ip, uint16_t hostport)
    {
        std::string sip = ConvertIp(ip);
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = ::inet_addr(sip.data());
        addr_.sin_port = htons(hostport);
    }

    const sockaddr_in& GetAddr() const
    {
        return addr_;
    }

    std::string GetIP() const
    {
        char tmp[32];
        const char* res = inet_ntop(AF_INET, &addr_.sin_addr,
                                    tmp, (socklen_t)(sizeof tmp));
        return std::string(res);
    }

    uint16_t GetPort() const
    {
        return ntohs(addr_.sin_port);
    }

    std::string ToString() const
    {
        char tmp[32];
        const char* res = inet_ntop(AF_INET, &addr_.sin_addr, tmp, (socklen_t)(sizeof tmp));

        return std::string(res) + ":" + std::to_string(ntohs(addr_.sin_port));
    }

    bool IsValid() const { return addr_.sin_family != 0; }

private:
    sockaddr_in  addr_;
};

extern const int kInvalid;

extern const int kTimeout;
extern const int kError;
extern const int kEof;

int CreateTCPSocket();
int CreateUDPSocket();
bool CreateSocketPair(int& readSock, int& writeSock);
void CloseSocket(int &sock);
void SetNonBlock(int sock, bool nonBlock = true);
void SetNodelay(int sock, bool enable = true);
void SetSndBuf(int sock, socklen_t size = 64 * 1024);
void SetRcvBuf(int sock, socklen_t size = 64 * 1024);
void SetReuseAddr(int sock);
bool GetLocalAddr(int sock, SocketAddr& );
bool GetPeerAddr(int sock, SocketAddr& );

in_addr_t GetLocalAddrInfo();

rlim_t GetMaxOpenFd();
bool SetMaxOpenFd(rlim_t maxfdPlus1);

} // end namespace ananas

#endif

